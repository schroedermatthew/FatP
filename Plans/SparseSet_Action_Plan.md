# SparseSet Deep Review & Action Plan

**Component:** SparseSet / SparseSetWithData  
**File:** `fat_p/SparseSet.h`  
**Lines:** 541  
**API Status:** `api_stability: in_work`  
**Layer:** Containers  
**Test File:** `test_SparseSet.cpp` (196 lines, 5 tests)  
**Documentation:** None found

**Review Date:** January 2026  
**Quality Score:** 6.5/10 (downgraded from 7 due to bug)  
**Bugs Found:** 1 P0  
**Effort to Candidate:** 6-8 hours

---

## ⚠️ CRITICAL: BUG FOUND

This component has a **confirmed bug** that must be fixed before candidate status.

| Bug | Severity | Type | Line(s) | Status |
|-----|----------|------|---------|--------|
| SparseSetWithData::erase() self-move-assignment | **P0** | Undefined behavior | 424-425 | ❌ Confirmed |

---

## 1. Architecture Analysis

### 1.1 Design Overview

```
SparseSet: O(1) set operations with dense iteration

┌─────────────────────────────────────────────────────────────────────┐
│                         SparseSet<T>                                 │
│                                                                     │
│  m_sparse (maps value → dense index):                               │
│  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐                     │
│  │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │...│                     │
│  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘                     │
│    │       │           │                                            │
│    │       │           └─ sparse[5] = 1 (value 5 is at dense[1])   │
│    │       └───────────── sparse[2] = 0 (value 2 is at dense[0])   │
│    └───────────────────── sparse[0] = 2 (value 0 is at dense[2])   │
│                                                                     │
│  m_dense (packed active values):                                    │
│  ┌───┬───┬───┐                                                     │
│  │ 2 │ 5 │ 0 │  ← Iteration goes through this (cache-friendly)     │
│  └───┴───┴───┘                                                     │
│    0   1   2                                                        │
│                                                                     │
│  size() = 3 (dense.size())                                          │
│  capacity() = 10+ (sparse.size())                                   │
└─────────────────────────────────────────────────────────────────────┘

Erase uses swap-with-last:
  Before erase(2):  dense = [2, 5, 0]
  After erase(2):   dense = [0, 5]     (0 moved from end to position 0)
```

### 1.2 Key Design Decisions

| Decision | Implementation | Rationale |
|----------|---------------|-----------|
| **Sparse + Dense arrays** | Two vectors | O(1) ops + cache-friendly iteration |
| **Swap-with-last erase** | Move last to erased position | O(1) removal |
| **Auto-grow sparse** | Resize on insert | No manual capacity management |
| **Dense iteration** | vector<T>::iterator | Standard iterator interface |

### 1.3 Complexity Analysis

| Operation | SparseSet | std::unordered_set | std::set |
|-----------|-----------|-------------------|----------|
| insert | O(1) amortized | O(1) amortized | O(log n) |
| erase | O(1) | O(1) amortized | O(log n) |
| contains | O(1) | O(1) average | O(log n) |
| iteration | O(n) cache-friendly | O(n) scattered | O(n) scattered |
| memory | O(max_value + n) | O(n) | O(n) |

### 1.4 Strengths

- O(1) insert, erase, contains
- Cache-friendly dense iteration
- Perfect for ECS (Entity Component System) patterns
- Simple, clean implementation
- No hash collisions (direct indexing)

### 1.5 Weaknesses

- High memory for very sparse data (stores entire sparse array)
- Dense order not stable (swap-with-last changes order)
- Self-move-assign bug in SparseSetWithData
- Limited test coverage (only 5 tests)

---

## 2. Bug Details

### 2.1 P0 Bug: SparseSetWithData::erase() Self-Move-Assignment

**Location:** Lines 421-430

```cpp
bool erase(T value)
{
    if (!contains(value))
        return false;

    T dense_idx = m_sparse[value];
    T last_value = m_dense.back();

    m_dense[dense_idx] = last_value;
    m_data[dense_idx] = std::move(m_data.back());  // BUG!
    m_sparse[last_value] = dense_idx;

    m_dense.pop_back();
    m_data.pop_back();
    return true;
}
```

**Problem:** When erasing the **last element** (the only element, or the element that happens to be at the end):
- `dense_idx == m_data.size() - 1`
- `m_data[dense_idx] = std::move(m_data.back())` becomes `m_data[last] = std::move(m_data[last])`
- This is **self-move-assignment**, which is undefined behavior for many types

**Demonstration:**
```cpp
SparseSetWithData<uint32_t, std::string> set;
set.insert(42, "answer");

// Erasing the only element triggers self-move:
set.erase(42);  // UNDEFINED BEHAVIOR: self-move-assign!
```

**Impact:**
- Undefined behavior for types that don't handle self-move
- Silent data corruption possible
- May crash on some implementations

**Fix:**
```cpp
bool erase(T value)
{
    if (!contains(value))
    {
        return false;
    }

    T dense_idx = m_sparse[value];
    size_t last_idx = m_dense.size() - 1;

    // Only swap if NOT erasing the last element
    if (dense_idx != static_cast<T>(last_idx))
    {
        T last_value = m_dense.back();
        m_dense[dense_idx] = last_value;
        m_data[dense_idx] = std::move(m_data.back());
        m_sparse[last_value] = dense_idx;
    }

    m_dense.pop_back();
    m_data.pop_back();
    return true;
}
```

**Effort:** 15 minutes

**Note:** The base `SparseSet::erase()` does NOT have this bug because it only copies a value type (T), not moves. Self-copy-assign of integers is fine.

---

## 3. Documentation Issue

### 3.1 Misleading "Stable Indices" Claim

**Location:** Lines 14-15 (header comment)

```cpp
* - Stable indices (elements don't move)
```

**Problem:** This is misleading. The **sparse indices** are stable, but the **dense array order** changes on erase due to swap-with-last.

**Fix documentation:**
```cpp
* - Stable sparse indices (value → dense mapping preserved)
* - Dense array order NOT stable (swap-with-last on erase)
```

---

## 4. Current Test Coverage

### 4.1 Test Summary

**Tests:** 5 ⚠️ (Very low)  
**Coverage gaps:** Critical paths untested

### 4.2 Tested ✅

- Basic insert/empty/contains
- Sparse indices (widely separated values)
- Basic erase
- Dense iteration
- SparseSetWithData basic operations

### 4.3 Not Tested ❌

| Test | Priority | Why Important |
|------|----------|---------------|
| **Erase last element** | **P0** | Triggers self-move bug |
| **Erase only element** | **P0** | Edge case of above |
| at() bounds checking | P1 | Exception safety |
| operator[] (no bounds) | P1 | Document behavior |
| reserve() | P2 | Capacity management |
| clear() | P2 | State reset |
| Large scale operations | P2 | Performance regression |
| Duplicate insert | P2 | Idempotency |
| Re-insert after erase | P2 | Slot reuse |
| dense()/sparse() accessors | P3 | Advanced API |

---

## 5. Missing Features

### 5.1 [[nodiscard]] Attributes

```cpp
// Should have [[nodiscard]]:
[[nodiscard]] bool contains(T value) const;
[[nodiscard]] bool empty() const noexcept;
[[nodiscard]] size_type size() const noexcept;
[[nodiscard]] size_type capacity() const noexcept;
[[nodiscard]] T operator[](size_type index) const;
[[nodiscard]] T at(size_type index) const;
[[nodiscard]] const std::vector<T>& dense() const noexcept;
[[nodiscard]] const std::vector<T>& sparse() const noexcept;

// For SparseSetWithData:
[[nodiscard]] Data& get(T value);
[[nodiscard]] const Data& get(T value) const;
[[nodiscard]] Data& data_at(size_type index);
[[nodiscard]] const Data& data_at(size_type index) const;
```

### 5.2 Debug Assertions

```cpp
T operator[](size_type index) const
{
    FATP_ASSERT(index < m_dense.size(), "operator[]: index out of range");
    return m_dense[index];
}
```

### 5.3 Explicit Move Operations (Rule of 5)

Currently uses implicit move. Should be explicit for clarity:

```cpp
SparseSet(SparseSet&&) noexcept = default;
SparseSet& operator=(SparseSet&&) noexcept = default;
```

---

## 6. Action Plan

### Phase 1: Critical Bug Fix (P0)

| # | Task | Effort | Priority |
|---|------|--------|----------|
| 1 | Fix SparseSetWithData::erase() self-move bug | 15m | **P0** |
| 2 | Add erase_last_element test | 10m | **P0** |
| 3 | Add erase_only_element test | 10m | **P0** |

### Phase 2: API Improvements (P1-P2)

| # | Task | Effort | Priority |
|---|------|--------|----------|
| 4 | Add [[nodiscard]] to 14 methods | 15m | P1 |
| 5 | Add debug assertion to operator[] | 5m | P2 |
| 6 | Add explicit defaulted move ops | 5m | P2 |
| 7 | Fix documentation about dense order stability | 10m | P2 |

### Phase 3: Comprehensive Tests

| # | Task | Effort | Priority |
|---|------|--------|----------|
| 8 | Add at_bounds_checking test | 10m | P1 |
| 9 | Add duplicate_insert test | 10m | P2 |
| 10 | Add reinsert_after_erase test | 10m | P2 |
| 11 | Add clear_and_reuse test | 10m | P2 |
| 12 | Add reserve_capacity test | 10m | P2 |
| 13 | Add large_scale test | 20m | P2 |
| 14 | Add dense_sparse_accessors test | 10m | P3 |

### Phase 4: Documentation

| # | Task | Effort | Priority |
|---|------|--------|----------|
| 15 | Write Overview.md | 1.5h | P2 |
| 16 | Write User_Manual.md | 2h | P2 |

---

## 7. Code Fixes

### 7.1 Fix: SparseSetWithData::erase()

```cpp
/**
 * @brief Erase element
 * @param value Index to erase
 * @return true if erased, false if not present
 * 
 * Uses swap-with-last for O(1) removal. Dense array order changes.
 */
bool erase(T value)
{
    if (!contains(value))
    {
        return false;
    }

    T dense_idx = m_sparse[value];
    size_t last_idx = m_dense.size() - 1;

    // Only swap if NOT erasing the last element
    // This prevents self-move-assignment which is UB for some types
    if (dense_idx != static_cast<T>(last_idx))
    {
        T last_value = m_dense.back();
        m_dense[dense_idx] = last_value;
        m_data[dense_idx] = std::move(m_data.back());
        m_sparse[last_value] = dense_idx;
    }

    m_dense.pop_back();
    m_data.pop_back();
    return true;
}
```

### 7.2 Add [[nodiscard]] Attributes

```cpp
// SparseSet
[[nodiscard]] bool insert(T value);
[[nodiscard]] bool erase(T value);
[[nodiscard]] bool contains(T value) const;
[[nodiscard]] size_type size() const noexcept;
[[nodiscard]] bool empty() const noexcept;
[[nodiscard]] size_type capacity() const noexcept;
[[nodiscard]] T operator[](size_type index) const;
[[nodiscard]] T at(size_type index) const;
[[nodiscard]] const std::vector<T>& dense() const noexcept;
[[nodiscard]] const std::vector<T>& sparse() const noexcept;

// SparseSetWithData (additional)
[[nodiscard]] bool insert(T value, const Data& data);
[[nodiscard]] bool insert(T value, Data&& data);
[[nodiscard]] Data& get(T value);
[[nodiscard]] const Data& get(T value) const;
[[nodiscard]] Data& data_at(size_type index);
[[nodiscard]] const Data& data_at(size_type index) const;
[[nodiscard]] const std::vector<Data>& data() const noexcept;
```

### 7.3 Add Debug Assertion

```cpp
T operator[](size_type index) const
{
#ifdef FATP_DEBUG
    if (index >= m_dense.size())
    {
        FATP_ASSERT(false, "SparseSet::operator[]: index out of range");
    }
#endif
    return m_dense[index];
}
```

### 7.4 Fix Documentation

```cpp
/**
 * @file SparseSet.h
 * @brief High-performance sparse set data structure for ECS and games
 *
 * Features:
 * - O(1) insertion, deletion, lookup, contains
 * - O(n) dense iteration over active elements
 * - Stable sparse indices (value-to-dense mapping preserved)
 * - Dense array order NOT stable (swap-with-last on erase)  // FIXED
 * - Memory efficient for sparse data
 * - Clear and reserve operations
 * - Compatible with range-based for loops
 */
```

---

## 8. Test Implementations

### 8.1 erase_last_element Test (P0)

```cpp
FATP_TEST_CASE(sparse_set_erase_last_element)
{
    SparseSetWithData<uint32_t, std::string> set;
    
    set.insert(10, "ten");
    set.insert(20, "twenty");
    set.insert(30, "thirty");  // This is the last element in dense array
    
    FATP_ASSERT_EQ(set.size(), 3u, "Should have 3 elements");
    
    // Erase the last element - this triggered self-move-assign bug
    bool erased = set.erase(30);
    
    FATP_ASSERT_TRUE(erased, "Should erase successfully");
    FATP_ASSERT_EQ(set.size(), 2u, "Should have 2 elements after erase");
    FATP_ASSERT_FALSE(set.contains(30), "Should not contain erased element");
    
    // Verify remaining elements are intact
    FATP_ASSERT_TRUE(set.contains(10), "Should still contain 10");
    FATP_ASSERT_TRUE(set.contains(20), "Should still contain 20");
    FATP_ASSERT_EQ(set.get(10), "ten", "Data for 10 should be intact");
    FATP_ASSERT_EQ(set.get(20), "twenty", "Data for 20 should be intact");
    
    return true;
}
```

### 8.2 erase_only_element Test (P0)

```cpp
FATP_TEST_CASE(sparse_set_erase_only_element)
{
    SparseSetWithData<uint32_t, std::string> set;
    
    set.insert(42, "answer");
    FATP_ASSERT_EQ(set.size(), 1u, "Should have 1 element");
    
    // Erase the only element - extreme edge case of erase-last
    bool erased = set.erase(42);
    
    FATP_ASSERT_TRUE(erased, "Should erase successfully");
    FATP_ASSERT_TRUE(set.empty(), "Should be empty after erase");
    FATP_ASSERT_FALSE(set.contains(42), "Should not contain erased element");
    
    // Verify can reuse the set
    set.insert(99, "new");
    FATP_ASSERT_EQ(set.size(), 1u, "Should have 1 element after re-insert");
    FATP_ASSERT_EQ(set.get(99), "new", "New data should be correct");
    
    return true;
}
```

### 8.3 at_bounds_checking Test

```cpp
FATP_TEST_CASE(sparse_set_at_bounds_checking)
{
    SparseSet<uint32_t> set;
    
    set.insert(100);
    set.insert(200);
    
    // Valid access
    FATP_ASSERT_NO_THROW(set.at(0), "at(0) should not throw");
    FATP_ASSERT_NO_THROW(set.at(1), "at(1) should not throw");
    
    // Out of bounds
    bool threw = false;
    try
    {
        set.at(2);
    }
    catch (const std::out_of_range&)
    {
        threw = true;
    }
    FATP_ASSERT_TRUE(threw, "at(2) should throw out_of_range");
    
    // Also test SparseSetWithData
    SparseSetWithData<uint32_t, int> set2;
    set2.insert(1, 10);
    
    threw = false;
    try
    {
        set2.get(999);  // Not present
    }
    catch (const std::out_of_range&)
    {
        threw = true;
    }
    FATP_ASSERT_TRUE(threw, "get(999) should throw out_of_range");
    
    return true;
}
```

### 8.4 duplicate_insert Test

```cpp
FATP_TEST_CASE(sparse_set_duplicate_insert)
{
    SparseSet<uint32_t> set;
    
    FATP_ASSERT_TRUE(set.insert(100), "First insert should return true");
    FATP_ASSERT_FALSE(set.insert(100), "Duplicate insert should return false");
    FATP_ASSERT_EQ(set.size(), 1u, "Size should still be 1");
    
    // With data
    SparseSetWithData<uint32_t, std::string> set2;
    FATP_ASSERT_TRUE(set2.insert(1, "first"), "First insert should return true");
    FATP_ASSERT_FALSE(set2.insert(1, "second"), "Duplicate insert should return false");
    FATP_ASSERT_EQ(set2.get(1), "first", "Original data should be preserved");
    
    return true;
}
```

### 8.5 reinsert_after_erase Test

```cpp
FATP_TEST_CASE(sparse_set_reinsert_after_erase)
{
    SparseSet<uint32_t> set;
    
    set.insert(100);
    set.insert(200);
    set.insert(300);
    
    // Erase middle element
    set.erase(200);
    FATP_ASSERT_FALSE(set.contains(200), "Should not contain 200");
    FATP_ASSERT_EQ(set.size(), 2u, "Size should be 2");
    
    // Re-insert same value
    FATP_ASSERT_TRUE(set.insert(200), "Re-insert should succeed");
    FATP_ASSERT_TRUE(set.contains(200), "Should contain 200 again");
    FATP_ASSERT_EQ(set.size(), 3u, "Size should be 3");
    
    // Verify all elements
    FATP_ASSERT_TRUE(set.contains(100), "Should contain 100");
    FATP_ASSERT_TRUE(set.contains(200), "Should contain 200");
    FATP_ASSERT_TRUE(set.contains(300), "Should contain 300");
    
    return true;
}
```

### 8.6 clear_and_reuse Test

```cpp
FATP_TEST_CASE(sparse_set_clear_and_reuse)
{
    SparseSet<uint32_t> set;
    
    set.insert(1);
    set.insert(2);
    set.insert(3);
    
    FATP_ASSERT_EQ(set.size(), 3u, "Should have 3 elements");
    
    set.clear();
    
    FATP_ASSERT_TRUE(set.empty(), "Should be empty after clear");
    FATP_ASSERT_EQ(set.size(), 0u, "Size should be 0");
    FATP_ASSERT_FALSE(set.contains(1), "Should not contain 1");
    FATP_ASSERT_FALSE(set.contains(2), "Should not contain 2");
    FATP_ASSERT_FALSE(set.contains(3), "Should not contain 3");
    
    // Reuse after clear
    set.insert(100);
    FATP_ASSERT_EQ(set.size(), 1u, "Should have 1 element after re-insert");
    FATP_ASSERT_TRUE(set.contains(100), "Should contain 100");
    
    return true;
}
```

### 8.7 reserve_capacity Test

```cpp
FATP_TEST_CASE(sparse_set_reserve_capacity)
{
    SparseSet<uint32_t> set;
    
    FATP_ASSERT_EQ(set.capacity(), 0u, "Initial capacity should be 0");
    
    set.reserve(1000);
    FATP_ASSERT_GE(set.capacity(), 1001u, "Capacity should be at least 1001");
    
    // Insert within reserved range - should not reallocate
    size_t cap_before = set.capacity();
    set.insert(500);
    FATP_ASSERT_EQ(set.capacity(), cap_before, "Capacity should not change");
    
    // Insert at reserved boundary
    set.insert(1000);
    FATP_ASSERT_TRUE(set.contains(1000), "Should contain 1000");
    
    return true;
}
```

### 8.8 large_scale Test

```cpp
FATP_TEST_CASE(sparse_set_large_scale)
{
    SparseSet<uint32_t> set;
    constexpr uint32_t COUNT = 10000;
    
    // Insert many elements (contiguous for memory efficiency)
    for (uint32_t i = 0; i < COUNT; ++i)
    {
        FATP_ASSERT_TRUE(set.insert(i), "Insert should succeed");
    }
    
    FATP_ASSERT_EQ(set.size(), COUNT, "Should have COUNT elements");
    
    // Verify all present
    for (uint32_t i = 0; i < COUNT; ++i)
    {
        FATP_ASSERT_TRUE(set.contains(i), "Should contain inserted value");
    }
    
    // Erase every other element
    for (uint32_t i = 0; i < COUNT; i += 2)
    {
        FATP_ASSERT_TRUE(set.erase(i), "Erase should succeed");
    }
    
    FATP_ASSERT_EQ(set.size(), COUNT / 2, "Should have half the elements");
    
    // Verify odd elements remain
    for (uint32_t i = 1; i < COUNT; i += 2)
    {
        FATP_ASSERT_TRUE(set.contains(i), "Odd values should remain");
    }
    
    // Verify even elements gone
    for (uint32_t i = 0; i < COUNT; i += 2)
    {
        FATP_ASSERT_FALSE(set.contains(i), "Even values should be gone");
    }
    
    return true;
}
```

---

## 9. Summary

### Effort Breakdown

| Phase | Tasks | Effort |
|-------|-------|--------|
| Bug Fix (P0) | 3 tasks | 35m |
| API Improvements | 4 tasks | 35m |
| Tests | 7 tests | 1.5h |
| Documentation | 2 docs | 3.5h |
| **Total** | | **~6.5h** |

### Quality Assessment (Post-Fix)

| Category | Before | After |
|----------|--------|-------|
| Correctness | 6/10 | 9/10 |
| Design | 8/10 | 8/10 |
| API | 6/10 | 8/10 |
| Tests | 4/10 | 8/10 |
| Documentation | 5/10 | 8/10 |
| **Overall** | **6.5/10** | **8.5/10** |

### Checklist for Candidate

| Requirement | Status |
|-------------|--------|
| Fix SparseSetWithData::erase() self-move bug | ☐ |
| Add erase_last_element test | ☐ |
| Add erase_only_element test | ☐ |
| Add [[nodiscard]] to all query methods | ☐ |
| Fix documentation about dense order stability | ☐ |
| Add at_bounds_checking test | ☐ |
| Add duplicate_insert test | ☐ |
| Add reinsert_after_erase test | ☐ |
| Add clear_and_reuse test | ☐ |
| Add large_scale test | ☐ |
| Write Overview.md | ☐ |
| Write User_Manual.md | ☐ |
| All tests pass | ☐ |

---

## 10. Comparison with Alternatives

| Feature | SparseSet | EnTT sparse_set | std::unordered_set |
|---------|-----------|-----------------|-------------------|
| Insert | O(1) | O(1) | O(1) avg |
| Erase | O(1) | O(1) | O(1) avg |
| Contains | O(1) | O(1) | O(1) avg |
| Iteration | O(n) contiguous | O(n) contiguous | O(n) scattered |
| Memory | O(max + n) | O(max + n) | O(n) |
| Header-only | ✅ | ✅ | ✅ (stdlib) |
| Dependencies | None | None | None |
| Associated data | SparseSetWithData | Separate storage | N/A |

**Conclusion:** SparseSet is competitive with industry-standard ECS implementations. The fix for the self-move bug and improved test coverage will make it production-ready.
