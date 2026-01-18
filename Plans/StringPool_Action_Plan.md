# StringPool Action Plan

**Component:** StringPool  
**Current Status:** `api_stability: in_work`  
**Target Status:** `api_stability: candidate`  
**Estimated Effort:** ~45 minutes  
**Risk Level:** Very Low (no breaking changes)

---

## Executive Summary

StringPool is already high-quality with 31 tests. Only minor additions needed:
- 8 `[[nodiscard]]` attributes
- 4 edge case tests

No bug fixes required. No API changes.

---

## Phase 1: Code Fixes (15 minutes)

### 1.1 Add `[[nodiscard]]` to StringPool Methods

**File:** `fat_p/StringPool.h`

```cpp
// Line 280 - intern(string_view)
[[nodiscard]] const char* intern(std::string_view str)

// Line 347 - intern(const char*)
[[nodiscard]] const char* intern(const char* str)

// Line 361 - intern(const std::string&)
[[nodiscard]] const char* intern(const std::string& str)

// Line 371 - contains
[[nodiscard]] bool contains(std::string_view str) const noexcept

// Line 386 - find
[[nodiscard]] const char* find(std::string_view str) const noexcept

// Line 400 - size
[[nodiscard]] size_t size() const noexcept

// Line 409 - empty
[[nodiscard]] bool empty() const noexcept

// Line 459 - stats
[[nodiscard]] StringPoolStats stats() const noexcept
```

### 1.2 Add `[[nodiscard]]` to StringHandle Methods

```cpp
// Line 529
[[nodiscard]] const char* get() const noexcept

// Line 534
[[nodiscard]] const char* c_str() const noexcept
```

---

## Phase 2: Test Additions (30 minutes)

### 2.1 Binary String Test

```cpp
FATP_TEST_CASE(binary_strings)
{
    StringPool<> pool;
    
    // String with embedded null bytes
    std::string binary1("hello\0world", 11);
    std::string binary2("hello\0world", 11);
    std::string binary3("hello\0other", 11);
    
    const char* s1 = pool.intern(std::string_view(binary1.data(), binary1.size()));
    const char* s2 = pool.intern(std::string_view(binary2.data(), binary2.size()));
    const char* s3 = pool.intern(std::string_view(binary3.data(), binary3.size()));
    
    FATP_ASSERT_EQ(s1, s2, "Identical binary strings should deduplicate");
    FATP_ASSERT_NE(s1, s3, "Different binary strings should not deduplicate");
    
    // Verify content preserved
    FATP_ASSERT_EQ(std::string_view(s1, 11), std::string_view(binary1.data(), 11),
                   "Binary content should be preserved");
    
    return true;
}
```

### 2.2 Empty Pool Edge Cases

```cpp
FATP_TEST_CASE(empty_pool_operations)
{
    StringPool<> pool;
    
    // Operations on empty pool
    FATP_ASSERT_TRUE(pool.empty(), "New pool should be empty");
    FATP_ASSERT_EQ(pool.size(), 0u, "New pool size should be 0");
    FATP_ASSERT_TRUE(!pool.contains("anything"), "Empty pool should not contain anything");
    FATP_ASSERT_EQ(pool.find("anything"), nullptr, "find() on empty pool should return nullptr");
    
    // Stats on empty pool
    auto stats = pool.stats();
    FATP_ASSERT_EQ(stats.unique_strings, 0u, "Empty pool unique_strings should be 0");
    FATP_ASSERT_EQ(stats.total_interns, 0u, "Empty pool total_interns should be 0");
    FATP_ASSERT_EQ(stats.content_bytes, 0u, "Empty pool content_bytes should be 0");
    FATP_ASSERT_EQ(stats.memory_saved, 0u, "Empty pool memory_saved should be 0");
    FATP_ASSERT_EQ(stats.hit_rate, 0.0, "Empty pool hit_rate should be 0");
    
    // Clear on empty pool should be safe
    pool.clear();
    FATP_ASSERT_TRUE(pool.empty(), "Pool should still be empty after clear");
    
    // Reset stats on empty pool
    pool.reset_stats();
    auto stats2 = pool.stats();
    FATP_ASSERT_EQ(stats2.unique_strings, 0u, "reset_stats on empty pool");
    
    return true;
}
```

### 2.3 Large Pool Stress Test

```cpp
FATP_TEST_CASE(large_pool_stress)
{
    StringPool<> pool;
    constexpr size_t NUM_UNIQUE = 10000;
    constexpr size_t NUM_DUPLICATES = 5;
    
    // Insert many unique strings
    std::vector<const char*> pointers;
    pointers.reserve(NUM_UNIQUE);
    
    for (size_t i = 0; i < NUM_UNIQUE; ++i)
    {
        std::string str = "unique_string_" + std::to_string(i);
        const char* ptr = pool.intern(str);
        pointers.push_back(ptr);
        
        // Verify content
        FATP_ASSERT_EQ(std::string_view(ptr), str, "Content should match");
    }
    
    FATP_ASSERT_EQ(pool.size(), NUM_UNIQUE, "Should have NUM_UNIQUE strings");
    
    // Insert duplicates
    for (size_t dup = 0; dup < NUM_DUPLICATES; ++dup)
    {
        for (size_t i = 0; i < NUM_UNIQUE; ++i)
        {
            std::string str = "unique_string_" + std::to_string(i);
            const char* ptr = pool.intern(str);
            FATP_ASSERT_EQ(ptr, pointers[i], "Duplicate should return same pointer");
        }
    }
    
    // Verify stats
    auto stats = pool.stats();
    FATP_ASSERT_EQ(stats.unique_strings, NUM_UNIQUE, "unique_strings count");
    FATP_ASSERT_EQ(stats.total_interns, NUM_UNIQUE * (NUM_DUPLICATES + 1), "total_interns count");
    
    // Hit rate should be NUM_UNIQUE * NUM_DUPLICATES / total
    double expected_hit_rate = static_cast<double>(NUM_UNIQUE * NUM_DUPLICATES) / 
                               static_cast<double>(NUM_UNIQUE * (NUM_DUPLICATES + 1));
    double tolerance = 0.001;
    FATP_ASSERT_TRUE(std::abs(stats.hit_rate - expected_hit_rate) < tolerance,
                     "Hit rate should match expected");
    
    return true;
}
```

### 2.4 StringHandle Edge Cases

```cpp
FATP_TEST_CASE(string_handle_edge_cases)
{
    StringPool<> pool;
    
    // Default-constructed handle
    StringHandle default_handle;
    FATP_ASSERT_EQ(default_handle.get(), nullptr, "Default handle should be nullptr");
    FATP_ASSERT_EQ(std::string_view(default_handle.c_str()), "", "Default c_str() should be empty");
    FATP_ASSERT_TRUE(!default_handle, "Default handle should be false");
    
    // Handle from nullptr
    StringHandle null_handle(nullptr);
    FATP_ASSERT_EQ(null_handle.get(), nullptr, "Null handle should be nullptr");
    FATP_ASSERT_TRUE(default_handle == null_handle, "Default and null should be equal");
    
    // Handle from interned string
    const char* ptr = pool.intern("test");
    StringHandle valid_handle(ptr);
    FATP_ASSERT_TRUE(valid_handle, "Valid handle should be true");
    FATP_ASSERT_NE(valid_handle, default_handle, "Valid should differ from default");
    
    // Handle comparison consistency
    StringHandle same_handle(ptr);
    FATP_ASSERT_TRUE(valid_handle == same_handle, "Same pointer should be equal");
    FATP_ASSERT_TRUE(!(valid_handle < same_handle), "Same pointer should not be less");
    FATP_ASSERT_TRUE(!(same_handle < valid_handle), "Same pointer should not be less");
    
    return true;
}
```

### 2.5 Update Test Runner

Add to `test_StringPool()`:

```cpp
FATP_RUN_TEST_NS(runner, stringpool, binary_strings);
FATP_RUN_TEST_NS(runner, stringpool, empty_pool_operations);
FATP_RUN_TEST_NS(runner, stringpool, large_pool_stress);
FATP_RUN_TEST_NS(runner, stringpool, string_handle_edge_cases);
```

---

## Phase 3: Optional Enhancements

### 3.1 Add `intern_with_status()` (Optional, 30 minutes)

```cpp
/**
 * @brief Intern a string and report whether it was new
 * @param str String to intern
 * @return Pair of (pointer to interned string, true if newly inserted)
 * 
 * Useful when you need to know if a string was already interned
 * without affecting statistics or for conditional processing.
 */
[[nodiscard]] std::pair<const char*, bool> intern_with_status(std::string_view str)
{
#if FATP_USE_TRANSPARENT_LOOKUP
    {
        typename SyncPolicy::ReadLock read_lock(sync_policy_.getLock());
        auto it = m_strings.find(str);
        if (it != m_strings.end())
        {
            detail::increment_stat(m_stats.total_interns);
            detail::increment_stat(m_stats.memory_saved, str.size() + 1);
            return {it->c_str(), false};
        }
    }

    typename SyncPolicy::WriteLock write_lock(sync_policy_.getLock());

    auto it = m_strings.find(str);
    if (it != m_strings.end())
    {
        detail::increment_stat(m_stats.total_interns);
        detail::increment_stat(m_stats.memory_saved, str.size() + 1);
        return {it->c_str(), false};
    }

    auto [inserted_it, success] = m_strings.emplace(str);
#else
    std::string temp(str);

    {
        typename SyncPolicy::ReadLock read_lock(sync_policy_.getLock());
        auto it = m_strings.find(temp);
        if (it != m_strings.end())
        {
            detail::increment_stat(m_stats.total_interns);
            detail::increment_stat(m_stats.memory_saved, str.size() + 1);
            return {it->c_str(), false};
        }
    }

    typename SyncPolicy::WriteLock write_lock(sync_policy_.getLock());

    auto it = m_strings.find(temp);
    if (it != m_strings.end())
    {
        detail::increment_stat(m_stats.total_interns);
        detail::increment_stat(m_stats.memory_saved, str.size() + 1);
        return {it->c_str(), false};
    }

    auto [inserted_it, success] = m_strings.insert(std::move(temp));
#endif

    if (success)
    {
        detail::increment_stat(m_stats.content_bytes, str.size() + 1);
    }

    detail::increment_stat(m_stats.total_interns);

    return {inserted_it->c_str(), success};
}
```

---

## Verification Plan

### Build and Test

```bash
cd /home/claude/FAT-P
g++ -std=c++17 -O2 -pthread -DENABLE_TEST_APPLICATION \
    -I fat_p tests/test_StringPool.cpp -o test_stringpool
./test_stringpool
```

### Expected Results

| Metric | Before | After |
|--------|--------|-------|
| Test cases | 31 | 35 |
| Assertions | ~150 | ~180 |
| Code coverage | ~90% | ~95% |

---

## Checklist

### Phase 1: Code Fixes (15m)

| Task | Effort | Status |
|------|--------|--------|
| Add `[[nodiscard]]` to `intern(string_view)` | 1m | ☐ |
| Add `[[nodiscard]]` to `intern(const char*)` | 1m | ☐ |
| Add `[[nodiscard]]` to `intern(const string&)` | 1m | ☐ |
| Add `[[nodiscard]]` to `contains()` | 1m | ☐ |
| Add `[[nodiscard]]` to `find()` | 1m | ☐ |
| Add `[[nodiscard]]` to `size()` | 1m | ☐ |
| Add `[[nodiscard]]` to `empty()` | 1m | ☐ |
| Add `[[nodiscard]]` to `stats()` | 1m | ☐ |
| Add `[[nodiscard]]` to `StringHandle::get()` | 1m | ☐ |
| Add `[[nodiscard]]` to `StringHandle::c_str()` | 1m | ☐ |

### Phase 2: Test Additions (30m)

| Task | Effort | Status |
|------|--------|--------|
| Add `binary_strings` test | 10m | ☐ |
| Add `empty_pool_operations` test | 10m | ☐ |
| Add `large_pool_stress` test | 10m | ☐ |
| Add `string_handle_edge_cases` test | 10m | ☐ |
| Update test runner | 5m | ☐ |
| Run all tests | 5m | ☐ |

### Phase 3: Optional (30m)

| Task | Effort | Status |
|------|--------|--------|
| Add `intern_with_status()` method | 30m | ☐ |
| Add test for `intern_with_status()` | 15m | ☐ |

### Final Steps

| Task | Status |
|------|--------|
| Update FATP_META to `candidate` | ☐ |
| Run benchmark suite | ☐ |
| Verify documentation current | ☐ |

---

## Summary

| Phase | Effort | Priority |
|-------|--------|----------|
| Code fixes | 15m | Required |
| Test additions | 30m | Required |
| Optional enhancements | 45m | Nice to have |
| **Total (required)** | **~45m** | - |

StringPool is already excellent. These changes polish it for candidate status.
