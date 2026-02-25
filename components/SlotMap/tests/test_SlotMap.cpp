/**
 * @file test_SlotMap.cpp
 * @brief Comprehensive unit tests for SlotMap.h
 */
/*
FATP_META:
  meta_version: 1
  component: SlotMap
  file_role: test
  path: components/SlotMap/tests/test_SlotMap.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for SlotMap."
  api_stability: in_work
  related:
    docs_search: "SlotMap"
    headers:
      - include/fat_p/SlotMap.h
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

#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "FatPTest.h"
#include "SlotMap.h"

namespace fat_p::testing::slotmap
{

// Test data structure
struct Entity
{
    int id;
    std::string name;
    float health;

    Entity(int i = 0, std::string n = "", float h = 0)
        : id(i)
        , name(std::move(n))
        , health(h)
    {
    }

    bool operator==(const Entity& other) const
    {
        return id == other.id && name == other.name && health == other.health;
    }
};

// =============================================================================
// Basic Operations
// =============================================================================

FATP_TEST_CASE(basic_insert_get)
{
    SlotMap<Entity> map;

    FATP_ASSERT_TRUE(map.empty(), "Map should start empty");
    FATP_ASSERT_EQ(map.size(), 0u, "Map should have size 0");

    auto handle = map.insert(Entity{1, "Alice", 100.0f});

    FATP_ASSERT_FALSE(map.empty(), "Map should not be empty");
    FATP_ASSERT_EQ(map.size(), 1u, "Map should have size 1");
    FATP_ASSERT_TRUE(map.is_valid(handle), "Handle should be valid");

    Entity* entity = map.get(handle);
    FATP_ASSERT_NOT_NULLPTR(entity, "Should get valid pointer");
    FATP_ASSERT_TRUE(entity->id == 1, "ID should match");
    FATP_ASSERT_TRUE(entity->name == "Alice", "Name should match");
    FATP_ASSERT_TRUE(entity->health == 100.0f, "Health should match");

    return true;
}

FATP_TEST_CASE(multiple_inserts)
{
    SlotMap<Entity> map;

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map.insert(Entity{2, "Bob", 80.0f});
    auto h3 = map.insert(Entity{3, "Charlie", 90.0f});

    FATP_ASSERT_EQ(map.size(), 3u, "Map should have 3 elements");

    FATP_ASSERT_TRUE(map.get(h1)->id == 1, "First entity ID should be 1");
    FATP_ASSERT_TRUE(map.get(h2)->id == 2, "Second entity ID should be 2");
    FATP_ASSERT_TRUE(map.get(h3)->id == 3, "Third entity ID should be 3");

    return true;
}

FATP_TEST_CASE(in_place_construction)
{
    SlotMap<Entity> map;

    // Insert using in-place construction (forwarding arguments)
    auto handle = map.insert(42, "InPlace", 75.0f);

    Entity* entity = map.get(handle);
    FATP_ASSERT_NOT_NULLPTR(entity, "Should get valid pointer");
    FATP_ASSERT_TRUE(entity->id == 42, "ID should be 42");
    FATP_ASSERT_TRUE(entity->name == "InPlace", "Name should match");
    FATP_ASSERT_TRUE(entity->health == 75.0f, "Health should match");

    return true;
}

FATP_TEST_CASE(erase)
{
    SlotMap<Entity> map;

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map.insert(Entity{2, "Bob", 80.0f});
    auto h3 = map.insert(Entity{3, "Charlie", 90.0f});

    FATP_ASSERT_EQ(map.size(), 3u, "Map should have 3 elements");

    bool erased = map.erase(h2);
    FATP_ASSERT_TRUE(erased, "Erase should succeed");
    FATP_ASSERT_EQ(map.size(), 2u, "Map should have 2 elements after erase");

    FATP_ASSERT_NULLPTR(map.get(h2), "Erased handle should return nullptr");
    FATP_ASSERT_FALSE(map.is_valid(h2), "Erased handle should be invalid");

    FATP_ASSERT_NOT_NULLPTR(map.get(h1), "Other handles should still be valid");
    FATP_ASSERT_NOT_NULLPTR(map.get(h3), "Other handles should still be valid");

    return true;
}

FATP_TEST_CASE(erase_invalid_handle)
{
    SlotMap<Entity> map;

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});

    // Erase once (valid)
    FATP_ASSERT_EQ(map.erase(h1), true, "First erase should succeed");

    // Erase again (invalid - already erased)
    FATP_ASSERT_EQ(map.erase(h1), false, "Second erase should fail");

    // Erase default handle
    SlotMap<Entity>::Handle null_handle;
    FATP_ASSERT_EQ(map.erase(null_handle), false, "Erasing null handle should fail");

    return true;
}

FATP_TEST_CASE(clear)
{
    SlotMap<Entity> map;

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map.insert(Entity{2, "Bob", 80.0f});
    auto h3 = map.insert(Entity{3, "Charlie", 90.0f});

    FATP_ASSERT_EQ(map.size(), 3u, "Map should have 3 elements");

    map.clear();

    FATP_ASSERT_TRUE(map.empty(), "Map should be empty after clear");
    FATP_ASSERT_EQ(map.size(), 0u, "Map should have size 0 after clear");

    // Old handles should be invalid
    FATP_ASSERT_FALSE(map.is_valid(h1), "Handle should be invalid after clear");
    FATP_ASSERT_FALSE(map.is_valid(h2), "Handle should be invalid after clear");
    FATP_ASSERT_FALSE(map.is_valid(h3), "Handle should be invalid after clear");

    return true;
}

// =============================================================================
// Generational Safety (ABA Problem)
// =============================================================================

FATP_TEST_CASE(generational_safety)
{
    SlotMap<Entity> map;

    // Insert and get handle
    auto handle1 = map.insert(Entity{1, "Alice", 100.0f});
    FATP_ASSERT_TRUE(map.is_valid(handle1), "Handle should be valid");

    // Erase
    map.erase(handle1);
    FATP_ASSERT_FALSE(map.is_valid(handle1), "Handle should be invalid after erase");
    FATP_ASSERT_NULLPTR(map.get(handle1), "Should return nullptr for invalid handle");

    // Insert new entity (reuses slot with new generation)
    auto handle2 = map.insert(Entity{2, "Bob", 80.0f});

    // Old handle should still be invalid (different generation)
    FATP_ASSERT_FALSE(map.is_valid(handle1), "Old handle should still be invalid");
    FATP_ASSERT_NULLPTR(map.get(handle1), "Old handle should return nullptr");

    // New handle should be valid
    FATP_ASSERT_TRUE(map.is_valid(handle2), "New handle should be valid");
    FATP_ASSERT_TRUE(map.get(handle2)->id == 2, "New handle should access correct entity");

    // Verify handles have same index but different generation
    FATP_ASSERT_EQ(handle1.index, handle2.index, "Slot should be reused");
    FATP_ASSERT_NE(handle1.generation, handle2.generation, "Generation should differ");

    return true;
}

FATP_TEST_CASE(aba_multiple_cycles)
{
    SlotMap<int> map;

    std::vector<SlotMap<int>::Handle> old_handles;

    // Create and destroy entities multiple times in the same slot
    for (int cycle = 0; cycle < 10; ++cycle)
    {
        auto handle = map.insert(cycle);
        old_handles.push_back(handle);
        map.erase(handle);
    }

    // Insert final entity
    auto final_handle = map.insert(999);

    // All old handles should be invalid
    for (size_t i = 0; i < old_handles.size(); ++i)
    {
        FATP_ASSERT_FALSE(map.is_valid(old_handles[i]), "Old handle should be invalid");
        FATP_ASSERT_NULLPTR(map.get(old_handles[i]), "Old handle should return nullptr");
    }

    // Final handle should be valid
    FATP_ASSERT_TRUE(map.is_valid(final_handle), "Final handle should be valid");
    FATP_ASSERT_EQ(*map.get(final_handle), 999, "Final value should be 999");

    return true;
}

// =============================================================================
// Handle Tests
// =============================================================================

FATP_TEST_CASE(handle_default_state)
{
    SlotMap<Entity>::Handle handle;

    FATP_ASSERT_TRUE(handle.is_null(), "Default handle should be null");
    FATP_ASSERT_FALSE(static_cast<bool>(handle), "Default handle should be falsy");
    FATP_ASSERT_EQ(handle.index, 0u, "Default index should be 0");
    FATP_ASSERT_EQ(handle.generation, 0u, "Default generation should be 0");

    return true;
}

FATP_TEST_CASE(handle_equality)
{
    SlotMap<Entity> map;

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map.insert(Entity{2, "Bob", 80.0f});

    FATP_ASSERT_TRUE(h1 == h1, "Handle should equal itself");
    FATP_ASSERT_TRUE(h1 != h2, "Different handles should not be equal");

    auto h1_copy = h1;
    FATP_ASSERT_TRUE(h1 == h1_copy, "Copied handle should be equal");

    // Handles with same index but different generation should not be equal
    map.erase(h1);
    auto h3 = map.insert(Entity{3, "Charlie", 90.0f});
    FATP_ASSERT_EQ(h1.index, h3.index, "Should reuse same slot");
    FATP_ASSERT_TRUE(h1 != h3, "Different generations should not be equal");

    return true;
}

FATP_TEST_CASE(handle_is_null_vs_is_valid)
{
    SlotMap<Entity> map;

    // Default handle: is_null() = true, is_valid() = false
    SlotMap<Entity>::Handle null_handle;
    FATP_ASSERT_TRUE(null_handle.is_null(), "Default handle is_null() should be true");
    FATP_ASSERT_FALSE(map.is_valid(null_handle), "Default handle is_valid() should be false");

    // Valid handle: is_null() = false, is_valid() = true
    auto valid_handle = map.insert(Entity{1, "Test", 100.0f});
    FATP_ASSERT_FALSE(valid_handle.is_null(), "Valid handle is_null() should be false");
    FATP_ASSERT_TRUE(map.is_valid(valid_handle), "Valid handle is_valid() should be true");

    // Erased handle: is_null() = false, is_valid() = false
    map.erase(valid_handle);
    FATP_ASSERT_FALSE(valid_handle.is_null(), "Erased handle is_null() should be false");
    FATP_ASSERT_FALSE(map.is_valid(valid_handle), "Erased handle is_valid() should be false");

    return true;
}

FATP_TEST_CASE(handle_operator_bool)
{
    SlotMap<Entity>::Handle null_handle;
    FATP_ASSERT_FALSE(null_handle, "Null handle should be falsy");

    SlotMap<Entity> map;
    auto handle = map.insert(Entity{1, "Test", 100.0f});
    FATP_ASSERT_TRUE(static_cast<bool>(handle), "Non-null handle should be truthy");

    // Note: operator bool only checks is_null(), not is_valid()
    map.erase(handle);
    FATP_ASSERT_TRUE(static_cast<bool>(handle), "Erased handle is still truthy (not null)");

    return true;
}

// =============================================================================
// Iteration Tests
// =============================================================================

FATP_TEST_CASE(iteration)
{
    SlotMap<Entity> map;

    (void)map.insert(Entity{1, "Alice", 100.0f});
    (void)map.insert(Entity{2, "Bob", 80.0f});
    (void)map.insert(Entity{3, "Charlie", 90.0f});

    int count = 0;
    int sum_ids = 0;

    for (const auto& entity : map)
    {
        ++count;
        sum_ids += entity.id;
    }

    FATP_ASSERT_EQ(count, 3, "Should iterate over 3 entities");
    FATP_ASSERT_EQ(sum_ids, 6, "Sum of IDs should be 6");

    return true;
}

FATP_TEST_CASE(iteration_empty)
{
    SlotMap<Entity> map;

    int count = 0;
    for (const auto& entity : map)
    {
        (void)entity;
        ++count;
    }

    FATP_ASSERT_EQ(count, 0, "Empty map iteration should have 0 elements");

    return true;
}

FATP_TEST_CASE(iteration_after_erase)
{
    SlotMap<Entity> map;

    (void)map.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map.insert(Entity{2, "Bob", 80.0f});
    (void)map.insert(Entity{3, "Charlie", 90.0f});

    // Erase middle element
    map.erase(h2);

    std::vector<int> ids;
    for (const auto& entity : map)
    {
        ids.push_back(entity.id);
    }

    FATP_ASSERT_EQ(ids.size(), 2u, "Should iterate over 2 entities");

    // Should have entities 1 and 3 (order may vary due to swap-and-pop)
    bool has_1 = std::find(ids.begin(), ids.end(), 1) != ids.end();
    bool has_3 = std::find(ids.begin(), ids.end(), 3) != ids.end();
    bool has_2 = std::find(ids.begin(), ids.end(), 2) != ids.end();

    FATP_ASSERT_TRUE(has_1, "Should have entity 1");
    FATP_ASSERT_TRUE(has_3, "Should have entity 3");
    FATP_ASSERT_FALSE(has_2, "Should not have entity 2");

    return true;
}

FATP_TEST_CASE(const_iteration)
{
    SlotMap<Entity> map;

    (void)map.insert(Entity{1, "Alice", 100.0f});
    (void)map.insert(Entity{2, "Bob", 80.0f});

    const SlotMap<Entity>& const_map = map;

    int count = 0;
    for (const auto& entity : const_map)
    {
        (void)entity;
        ++count;
    }

    FATP_ASSERT_EQ(count, 2, "Const iteration should work");

    // Test cbegin/cend
    count = 0;
    for (auto it = map.cbegin(); it != map.cend(); ++it)
    {
        ++count;
    }

    FATP_ASSERT_EQ(count, 2, "cbegin/cend should work");

    return true;
}

FATP_TEST_CASE(entries_iteration)
{
    SlotMap<Entity> map;

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map.insert(Entity{2, "Bob", 80.0f});
    auto h3 = map.insert(Entity{3, "Charlie", 90.0f});

    std::vector<SlotMap<Entity>::Handle> handles;
    std::vector<int> ids;

    for (auto entry : map.entries())
    {
        handles.push_back(entry.handle);
        ids.push_back(entry.value.id);
    }

    FATP_ASSERT_EQ(handles.size(), 3u, "Should have 3 entries");
    FATP_ASSERT_EQ(ids.size(), 3u, "Should have 3 IDs");

    // All handles should be valid
    for (const auto& h : handles)
    {
        FATP_ASSERT_TRUE(map.is_valid(h), "Handle from entries() should be valid");
    }

    // Should contain all our original handles
    bool found_h1 = std::find(handles.begin(), handles.end(), h1) != handles.end();
    bool found_h2 = std::find(handles.begin(), handles.end(), h2) != handles.end();
    bool found_h3 = std::find(handles.begin(), handles.end(), h3) != handles.end();

    FATP_ASSERT_TRUE(found_h1, "Should find handle 1");
    FATP_ASSERT_TRUE(found_h2, "Should find handle 2");
    FATP_ASSERT_TRUE(found_h3, "Should find handle 3");

    return true;
}

FATP_TEST_CASE(entries_const_iteration)
{
    SlotMap<Entity> map;

    (void)map.insert(Entity{1, "Alice", 100.0f});
    (void)map.insert(Entity{2, "Bob", 80.0f});

    const SlotMap<Entity>& const_map = map;

    int count = 0;
    for (auto entry : const_map.entries())
    {
        FATP_ASSERT_TRUE(const_map.is_valid(entry.handle), "Handle should be valid");
        ++count;
    }

    FATP_ASSERT_EQ(count, 2, "Const entries iteration should work");

    return true;
}

FATP_TEST_CASE(entries_empty)
{
    SlotMap<Entity> map;

    int count = 0;
    for (auto entry : map.entries())
    {
        (void)entry;
        ++count;
    }

    FATP_ASSERT_EQ(count, 0, "Empty entries iteration should have 0 elements");

    return true;
}

FATP_TEST_CASE(entries_modification)
{
    SlotMap<Entity> map;

    (void)map.insert(Entity{1, "Alice", 100.0f});
    (void)map.insert(Entity{2, "Bob", 80.0f});

    // Modify values through entries
    for (auto entry : map.entries())
    {
        entry.value.health += 10.0f;
    }

    // Verify modification
    for (const auto& entity : map)
    {
        FATP_ASSERT_TRUE(entity.health == 110.0f || entity.health == 90.0f, "Health should be modified");
    }

    return true;
}

// =============================================================================
// Copy/Move Tests
// =============================================================================

FATP_TEST_CASE(copy_construction)
{
    SlotMap<Entity> map1;

    auto h1 = map1.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map1.insert(Entity{2, "Bob", 80.0f});

    SlotMap<Entity> map2(map1);

    // Both maps should have same size
    FATP_ASSERT_EQ(map2.size(), 2u, "Copied map should have 2 elements");

    // Handles from original should work in copy
    FATP_ASSERT_TRUE(map2.is_valid(h1), "Handle should be valid in copy");
    FATP_ASSERT_TRUE(map2.is_valid(h2), "Handle should be valid in copy");

    FATP_ASSERT_TRUE(map2.get(h1)->id == 1, "Data should be copied correctly");
    FATP_ASSERT_TRUE(map2.get(h2)->id == 2, "Data should be copied correctly");

    // Modifying copy should not affect original
    map2.get(h1)->health = 50.0f;
    FATP_ASSERT_TRUE(map1.get(h1)->health == 100.0f, "Original should be unchanged");
    FATP_ASSERT_TRUE(map2.get(h1)->health == 50.0f, "Copy should be modified");

    return true;
}

FATP_TEST_CASE(copy_assignment)
{
    SlotMap<Entity> map1;
    auto h1 = map1.insert(Entity{1, "Alice", 100.0f});

    SlotMap<Entity> map2;
    (void)map2.insert(Entity{99, "Temp", 0.0f});

    map2 = map1;

    FATP_ASSERT_EQ(map2.size(), 1u, "Assigned map should have 1 element");
    FATP_ASSERT_TRUE(map2.is_valid(h1), "Handle should be valid after assignment");
    FATP_ASSERT_TRUE(map2.get(h1)->id == 1, "Data should be copied correctly");

    return true;
}

FATP_TEST_CASE(move_construction)
{
    SlotMap<Entity> map1;
    auto h1 = map1.insert(Entity{1, "Alice", 100.0f});

    SlotMap<Entity> map2(std::move(map1));

    FATP_ASSERT_EQ(map2.size(), 1u, "Moved-to map should have 1 element");
    FATP_ASSERT_TRUE(map2.is_valid(h1), "Handle should be valid in moved-to map");
    FATP_ASSERT_TRUE(map2.get(h1)->id == 1, "Data should be moved correctly");

    // Original should be empty (moved-from state)
    FATP_ASSERT_TRUE(map1.empty(), "Moved-from map should be empty");

    return true;
}

FATP_TEST_CASE(move_assignment)
{
    SlotMap<Entity> map1;
    auto h1 = map1.insert(Entity{1, "Alice", 100.0f});

    SlotMap<Entity> map2;
    (void)map2.insert(Entity{99, "Temp", 0.0f});

    map2 = std::move(map1);

    FATP_ASSERT_EQ(map2.size(), 1u, "Moved-to map should have 1 element");
    FATP_ASSERT_TRUE(map2.is_valid(h1), "Handle should be valid after move assignment");
    FATP_ASSERT_TRUE(map2.get(h1)->id == 1, "Data should be moved correctly");

    return true;
}

// =============================================================================
// Capacity Tests
// =============================================================================

FATP_TEST_CASE(reserve)
{
    SlotMap<Entity> map;

    map.reserve(100);
    FATP_ASSERT_TRUE(map.capacity() >= 100, "Capacity should be at least 100");
    FATP_ASSERT_TRUE(map.empty(), "Map should still be empty after reserve");

    return true;
}

FATP_TEST_CASE(slot_count_tracking)
{
    SlotMap<Entity> map;

    FATP_ASSERT_EQ(map.size(), 0u, "Initial slot count should be 0");
    FATP_ASSERT_EQ(map.free_slot_count(), 0u, "Initial free slot count should be 0");

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    (void)map.insert(Entity{2, "Bob", 80.0f});

    FATP_ASSERT_EQ(map.size(), 2u, "Should have 2 slots");
    FATP_ASSERT_EQ(map.free_slot_count(), 0u, "Should have 0 free slots");

    map.erase(h1);

    FATP_ASSERT_EQ(map.slot_count(), 2u, "Slot count unchanged after erase");
    FATP_ASSERT_EQ(map.free_slot_count(), 1u, "Should have 1 free slot");

    // Insert should reuse the free slot
    auto h3 = map.insert(Entity{3, "Charlie", 90.0f});
    (void)h3;

    FATP_ASSERT_EQ(map.slot_count(), 2u, "Slot count should still be 2 (reused)");
    FATP_ASSERT_EQ(map.free_slot_count(), 0u, "Should have 0 free slots");

    return true;
}

// =============================================================================
// Unchecked Access Tests
// =============================================================================

FATP_TEST_CASE(get_unchecked_basic)
{
    SlotMap<Entity> map;

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map.insert(Entity{2, "Bob", 80.0f});

    // get_unchecked returns reference, not pointer
    Entity& e1 = map.get_unchecked(h1);
    FATP_ASSERT_EQ(e1.id, 1, "Should access correct entity");
    FATP_ASSERT_EQ(e1.name, "Alice", "Name should match");

    // Modify through unchecked access
    e1.health = 50.0f;
    FATP_ASSERT_TRUE(map.get(h1)->health == 50.0f, "Modification should persist");

    // Const version
    const SlotMap<Entity>& const_map = map;
    const Entity& e2 = const_map.get_unchecked(h2);
    FATP_ASSERT_EQ(e2.id, 2, "Const access should work");

    return true;
}

FATP_TEST_CASE(get_unchecked_performance_pattern)
{
    SlotMap<int> map;
    std::vector<SlotMap<int>::Handle> handles;

    // Insert elements
    for (int i = 0; i < 100; ++i)
    {
        handles.push_back(map.insert(i * 10));
    }

    // HPC pattern: validate once, then use unchecked access
    int sum = 0;
    for (const auto& h : handles)
    {
        if (map.is_valid(h))
        {
            // After validation, use unchecked for speed
            sum += map.get_unchecked(h);
        }
    }

    FATP_ASSERT_EQ(sum, 49500, "Sum should be 0+10+20+...+990 = 49500");

    return true;
}

// =============================================================================
// Stress Tests
// =============================================================================

FATP_TEST_CASE(stress_insert_erase)
{
    SlotMap<int> map;
    std::vector<SlotMap<int>::Handle> handles;

    // Insert many elements
    for (int i = 0; i < 1000; ++i)
    {
        handles.push_back(map.insert(i));
    }

    FATP_ASSERT_EQ(map.size(), 1000u, "Map should have 1000 elements");

    // Erase every other element
    for (size_t i = 0; i < handles.size(); i += 2)
    {
        map.erase(handles[i]);
    }

    FATP_ASSERT_EQ(map.size(), 500u, "Map should have 500 elements after erasure");

    // Verify remaining elements
    for (size_t i = 1; i < handles.size(); i += 2)
    {
        int* val = map.get(handles[i]);
        FATP_ASSERT_NOT_NULLPTR(val, "Handle should be valid");
        FATP_ASSERT_EQ(*val, static_cast<int>(i), "Value should match");
    }

    // Verify erased elements are invalid
    for (size_t i = 0; i < handles.size(); i += 2)
    {
        FATP_ASSERT_NULLPTR(map.get(handles[i]), "Erased handle should be invalid");
    }

    return true;
}

FATP_TEST_CASE(stress_slot_reuse)
{
    SlotMap<int> map;

    // Insert and erase repeatedly to stress slot reuse
    for (int round = 0; round < 100; ++round)
    {
        std::vector<SlotMap<int>::Handle> handles;

        for (int i = 0; i < 10; ++i)
        {
            handles.push_back(map.insert(round * 10 + i));
        }

        // Erase all
        for (auto h : handles)
        {
            map.erase(h);
        }
    }

    FATP_ASSERT_TRUE(map.empty(), "Map should be empty");
    // Slot count should be 10 (all reused)
    FATP_ASSERT_EQ(map.slot_count(), 10u, "Only 10 slots should ever be allocated");

    return true;
}

// =============================================================================
// CRITICAL BUG FIX TESTS
// =============================================================================

// Test: clear() must not cause ABA violation
// Bug: Original clear() wiped mSlots, resetting generations to 0.
// Fix: Increment generations on clear, don't reset them.
FATP_TEST_CASE(clear_aba_fix)
{
    SlotMap<int> map;

    auto h1 = map.insert(42);
    FATP_ASSERT_TRUE(map.is_valid(h1), "Handle should be valid before erase");

    map.erase(h1);
    FATP_ASSERT_FALSE(map.is_valid(h1), "Handle should be invalid after erase");

    map.clear();

    // Insert new element - may reuse same slot index
    auto h2 = map.insert(99);

    // CRITICAL: Old handle h1 must NOT validate after clear!
    FATP_ASSERT_FALSE(map.is_valid(h1), "Old handle must remain invalid after clear");

    // New handle should work
    FATP_ASSERT_TRUE(map.is_valid(h2), "New handle should be valid");
    FATP_ASSERT_EQ(*map.get(h2), 99, "New handle should point to correct data");

    return true;
}

// Test: Generation wrap must skip 0 to preserve is_null() semantics
// Bug: uint32_t wrap to 0 would make is_null() return true for valid handles
// Fix: if (++generation == 0) generation = 1;
FATP_TEST_CASE(generation_wrap_skips_zero)
{
    SlotMap<int> map;

    // Insert many elements and verify none have generation 0
    std::vector<SlotMap<int>::Handle> handles;
    for (int i = 0; i < 100; ++i)
    {
        handles.push_back(map.insert(i));
    }

    // All handles should have non-zero generation
    for (const auto& h : handles)
    {
        FATP_ASSERT_FALSE(h.is_null(), "Valid handle should not be null");
        FATP_ASSERT_NE(h.generation, 0u, "Valid handle generation should never be 0");
    }

    // Erase and reinsert multiple times on same slots
    for (int cycle = 0; cycle < 10; ++cycle)
    {
        for (auto& h : handles)
        {
            map.erase(h);
        }
        handles.clear();

        for (int i = 0; i < 100; ++i)
        {
            auto new_h = map.insert(i);
            FATP_ASSERT_FALSE(new_h.is_null(), "New handle should not be null");
            FATP_ASSERT_NE(new_h.generation, 0u, "New handle generation should never be 0");
            handles.push_back(new_h);
        }
    }

    return true;
}

// =============================================================================
// NEW API TESTS
// =============================================================================

// Test: at() throws on invalid handle
FATP_TEST_CASE(at_throws_on_invalid)
{
    SlotMap<int> map;
    auto h = map.insert(42);

    // Valid handle works
    FATP_ASSERT_EQ(map.at(h), 42, "at() should return correct value");

    // Erase it
    map.erase(h);

    // Now at() should throw
    bool threw = false;
    try
    {
        (void)map.at(h);
    }
    catch (const std::out_of_range&)
    {
        threw = true;
    }
    FATP_ASSERT_TRUE(threw, "at() should throw on invalid handle");

    return true;
}

// Test: contains() is alias for is_valid()
FATP_TEST_CASE(contains_alias)
{
    SlotMap<int> map;
    auto h = map.insert(42);

    FATP_ASSERT_TRUE(map.contains(h), "contains() should return true for valid handle");

    map.erase(h);

    FATP_ASSERT_FALSE(map.contains(h), "contains() should return false for erased handle");

    SlotMap<int>::Handle null_handle;
    FATP_ASSERT_FALSE(map.contains(null_handle), "contains() should return false for null handle");

    return true;
}

// Test: emplace() is alias for insert()
FATP_TEST_CASE(emplace_alias)
{
    SlotMap<Entity> map;

    auto h = map.emplace(42, "Emplaced", 100.0f);

    FATP_ASSERT_TRUE(map.is_valid(h), "emplace() should return valid handle");
    FATP_ASSERT_TRUE(map.get(h)->id == 42, "emplaced entity should have correct id");
    FATP_ASSERT_TRUE(map.get(h)->name == "Emplaced", "emplaced entity should have correct name");

    return true;
}

// Test: Handle can be used in std::unordered_map (requires std::hash)
FATP_TEST_CASE(handle_hashable)
{
    SlotMap<int> map;

    auto h1 = map.insert(1);
    auto h2 = map.insert(2);
    auto h3 = map.insert(3);

    // Should be usable in unordered_map
    std::unordered_map<SlotMapHandle, std::string> lookup;
    lookup[h1] = "one";
    lookup[h2] = "two";
    lookup[h3] = "three";

    FATP_ASSERT_EQ(lookup[h1], "one", "Hash lookup should work for h1");
    FATP_ASSERT_EQ(lookup[h2], "two", "Hash lookup should work for h2");
    FATP_ASSERT_EQ(lookup[h3], "three", "Hash lookup should work for h3");

    return true;
}

// Test: Handle comparison operators for std::set/map
FATP_TEST_CASE(handle_ordering)
{
    SlotMapHandle h1{0, 1};
    SlotMapHandle h2{0, 2};
    SlotMapHandle h3{1, 1};

    // Less-than comparison
    FATP_ASSERT_TRUE(h1 < h2, "Same index, lower generation should be less");
    FATP_ASSERT_TRUE(h1 < h3, "Lower index should be less");
    FATP_ASSERT_FALSE((h2 < h1), "Higher generation should not be less");

    // Should work in std::set
    std::set<SlotMapHandle> handles;
    handles.insert(h1);
    handles.insert(h2);
    handles.insert(h3);
    FATP_ASSERT_EQ(handles.size(), 3u, "Set should contain all 3 unique handles");

    return true;
}

// Test: swap() member function
FATP_TEST_CASE(swap_member)
{
    SlotMap<int> map1;
    auto h1 = map1.insert(1);
    (void)map1.insert(2);

    SlotMap<int> map2;
    auto h3 = map2.insert(3);

    map1.swap(map2);

    FATP_ASSERT_EQ(map1.size(), 1u, "map1 should have 1 element after swap");
    FATP_ASSERT_EQ(map2.size(), 2u, "map2 should have 2 elements after swap");

    // Handles track their original map's data
    FATP_ASSERT_TRUE(map1.is_valid(h3), "h3 should be valid in map1 after swap");
    FATP_ASSERT_TRUE(map2.is_valid(h1), "h1 should be valid in map2 after swap");

    return true;
}

// =============================================================================
// insert_at() Tests
// =============================================================================

// Test: insert_at honours hint when slot is free
FATP_TEST_CASE(insert_at_honours_hint_free_slot)
{
    SlotMap<int> map;

    // Insert at a slot that has never been used
    auto h = map.insert_at(5, 42);

    FATP_ASSERT_EQ(h.index, 5u, "Hint should be honoured for an unused slot");
    FATP_ASSERT_TRUE(map.is_valid(h), "Returned handle should be valid");
    FATP_ASSERT_EQ(*map.get(h), 42, "Value should be correct");

    return true;
}

// Test: insert_at honours hint when slot was previously erased
FATP_TEST_CASE(insert_at_honours_hint_erased_slot)
{
    SlotMap<int> map;

    // Build up a slot at index 2, then erase it
    (void)map.insert(10); // index 0
    (void)map.insert(20); // index 1
    auto h2 = map.insert(30); // index 2
    FATP_ASSERT_EQ(h2.index, 2u, "Precondition: h2 at index 2");

    map.erase(h2);

    // Now insert_at(2, ...) should land on the free slot
    auto h_new = map.insert_at(2, 99);

    FATP_ASSERT_EQ(h_new.index, 2u, "Hint should be honoured for an erased slot");
    FATP_ASSERT_TRUE(map.is_valid(h_new), "New handle should be valid");
    FATP_ASSERT_FALSE(map.is_valid(h2), "Old handle must remain invalid (new generation)");
    FATP_ASSERT_EQ(*map.get(h_new), 99, "Value should be correct");

    return true;
}

// Test: insert_at falls back to normal insert when slot is occupied
FATP_TEST_CASE(insert_at_fallback_when_occupied)
{
    SlotMap<int> map;

    auto h0 = map.insert(100); // index 0
    FATP_ASSERT_EQ(h0.index, 0u, "Precondition: h0 at index 0");

    // Attempt to insert at the already-occupied index 0
    auto h_new = map.insert_at(0, 200);

    // Fallback: result goes somewhere else
    FATP_ASSERT_NE(h_new.index, 0u, "Should not use the occupied slot");
    FATP_ASSERT_TRUE(map.is_valid(h_new), "Fallback handle should be valid");
    FATP_ASSERT_EQ(*map.get(h_new), 200, "Value should be correct");

    // Original element must be untouched
    FATP_ASSERT_TRUE(map.is_valid(h0), "Original handle should remain valid");
    FATP_ASSERT_EQ(*map.get(h0), 100, "Original value should be unchanged");

    return true;
}

// Test: insert_at grows the slot array when hint is beyond current range
FATP_TEST_CASE(insert_at_extends_slot_array)
{
    SlotMap<int> map;

    // Map is empty; hint index 10 is well beyond slot_count() == 0
    auto h = map.insert_at(10, 77);

    FATP_ASSERT_EQ(h.index, 10u, "Hint beyond range should allocate new slots");
    FATP_ASSERT_TRUE(map.is_valid(h), "Handle should be valid");
    FATP_ASSERT_EQ(*map.get(h), 77, "Value should be correct");

    // Slots 0–9 should be in the free list (allocated but unused)
    FATP_ASSERT_EQ(map.slot_count(), 11u, "slot_count should be hint+1");
    FATP_ASSERT_EQ(map.free_slot_count(), 10u, "Slots 0-9 should be free");

    return true;
}

// Test: intermediate free slots from insert_at extension are reusable
FATP_TEST_CASE(insert_at_intermediate_slots_reusable)
{
    SlotMap<int> map;

    // Create gap slots 0-4 by hinting at index 5
    (void)map.insert_at(5, 500);

    // Normal inserts should reclaim the free slots 0-4
    std::vector<SlotMap<int>::Handle> handles;
    for (int i = 0; i < 5; ++i)
    {
        handles.push_back(map.insert(i * 10));
    }

    // No new slots should have been allocated beyond 6
    FATP_ASSERT_EQ(map.slot_count(), 6u, "All 6 slots should be in use now");
    FATP_ASSERT_EQ(map.free_slot_count(), 0u, "No free slots should remain");

    // All handles should be valid with correct values
    for (size_t i = 0; i < 5; ++i)
    {
        FATP_ASSERT_TRUE(map.is_valid(handles[i]), "Handle should be valid");
        FATP_ASSERT_EQ(*map.get(handles[i]), static_cast<int>(i) * 10, "Value should match");
    }

    return true;
}

// Test: insert_at preserves ABA safety (new generation after erase + insert_at)
FATP_TEST_CASE(insert_at_aba_safety)
{
    SlotMap<int> map;

    auto h_orig = map.insert(1); // index 0, gen 1
    map.erase(h_orig);           // index 0 freed, gen incremented

    auto h_new = map.insert_at(0, 2); // should land at index 0, new gen

    FATP_ASSERT_EQ(h_new.index, 0u, "Should reuse slot 0");
    FATP_ASSERT_NE(h_new.generation, h_orig.generation, "Generations must differ");
    FATP_ASSERT_FALSE(map.is_valid(h_orig), "Old handle must be invalid");
    FATP_ASSERT_TRUE(map.is_valid(h_new), "New handle must be valid");

    return true;
}

// Test: insert_at with in-place construction (variadic args)
FATP_TEST_CASE(insert_at_variadic_args)
{
    SlotMap<Entity> map;

    auto h = map.insert_at(3, 42, "Hinted", 75.0f);

    FATP_ASSERT_EQ(h.index, 3u, "Hint should be honoured");
    FATP_ASSERT_TRUE(map.is_valid(h), "Handle should be valid");
    FATP_ASSERT_EQ(map.get(h)->id, 42, "ID should match");
    FATP_ASSERT_EQ(map.get(h)->name, "Hinted", "Name should match");

    return true;
}

// Test: insert_at snapshot-restore round-trip
FATP_TEST_CASE(insert_at_snapshot_restore)
{
    SlotMap<int> map;

    // Simulate a serialised session: insert some items, record handles
    auto h0 = map.insert(10);
    auto h1 = map.insert(20);
    auto h2 = map.insert(30);
    (void)map.erase(h1); // leave a gap

    // Save (index, value) pairs as "snapshot"
    struct Snap { uint32_t index; int value; };
    std::vector<Snap> snapshot;
    for (auto entry : map.entries())
    {
        snapshot.push_back({entry.handle.index, entry.value});
    }

    // Restore into a fresh map
    SlotMap<int> restored;
    std::vector<SlotMap<int>::Handle> restored_handles;
    for (auto& s : snapshot)
    {
        restored_handles.push_back(restored.insert_at(s.index, s.value));
    }

    // Every restored handle must sit at its original index
    for (size_t i = 0; i < snapshot.size(); ++i)
    {
        FATP_ASSERT_EQ(restored_handles[i].index, snapshot[i].index,
                       "Restored handle index must match snapshot");
        FATP_ASSERT_EQ(*restored.get(restored_handles[i]), snapshot[i].value,
                       "Restored value must match snapshot");
    }

    // h0 and h2's indices should still be valid in restored map
    FATP_ASSERT_TRUE(restored.is_valid(restored_handles[0]), "First restored handle should be valid");
    (void)h0; (void)h2;

    return true;
}

// Test: shrink_to_fit() reduces memory
FATP_TEST_CASE(shrink_to_fit)
{
    SlotMap<int> map;
    map.reserve(1000);

    FATP_ASSERT_TRUE(map.capacity() >= 1000, "Capacity should be at least 1000 after reserve");

    (void)map.insert(1);
    (void)map.insert(2);

    map.shrink_to_fit();

    // Capacity may still be >= 2, but should be reduced from 1000
    // (Implementation-defined, so we just verify it doesn't crash)
    FATP_ASSERT_EQ(map.size(), 2u, "Size should still be 2 after shrink_to_fit");

    return true;
}

// =============================================================================
// Benchmarks
// =============================================================================
} // namespace fat_p::testing::slotmap

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_SlotMap()
{
    FATP_PRINT_HEADER(SLOT MAP)

    TestRunner runner;

    // Basic operations
    FATP_RUN_TEST_NS(runner, slotmap, basic_insert_get);
    FATP_RUN_TEST_NS(runner, slotmap, multiple_inserts);
    FATP_RUN_TEST_NS(runner, slotmap, in_place_construction);
    FATP_RUN_TEST_NS(runner, slotmap, erase);
    FATP_RUN_TEST_NS(runner, slotmap, erase_invalid_handle);
    FATP_RUN_TEST_NS(runner, slotmap, clear);

    // Generational safety
    FATP_RUN_TEST_NS(runner, slotmap, generational_safety);
    FATP_RUN_TEST_NS(runner, slotmap, aba_multiple_cycles);

    // Handle tests
    FATP_RUN_TEST_NS(runner, slotmap, handle_default_state);
    FATP_RUN_TEST_NS(runner, slotmap, handle_equality);
    FATP_RUN_TEST_NS(runner, slotmap, handle_is_null_vs_is_valid);
    FATP_RUN_TEST_NS(runner, slotmap, handle_operator_bool);

    // Iteration tests
    FATP_RUN_TEST_NS(runner, slotmap, iteration);
    FATP_RUN_TEST_NS(runner, slotmap, iteration_empty);
    FATP_RUN_TEST_NS(runner, slotmap, iteration_after_erase);
    FATP_RUN_TEST_NS(runner, slotmap, const_iteration);
    FATP_RUN_TEST_NS(runner, slotmap, entries_iteration);
    FATP_RUN_TEST_NS(runner, slotmap, entries_const_iteration);
    FATP_RUN_TEST_NS(runner, slotmap, entries_empty);
    FATP_RUN_TEST_NS(runner, slotmap, entries_modification);

    // Copy/move tests
    FATP_RUN_TEST_NS(runner, slotmap, copy_construction);
    FATP_RUN_TEST_NS(runner, slotmap, copy_assignment);
    FATP_RUN_TEST_NS(runner, slotmap, move_construction);
    FATP_RUN_TEST_NS(runner, slotmap, move_assignment);

    // Capacity tests
    FATP_RUN_TEST_NS(runner, slotmap, reserve);
    FATP_RUN_TEST_NS(runner, slotmap, slot_count_tracking);

    // Unchecked access tests
    FATP_RUN_TEST_NS(runner, slotmap, get_unchecked_basic);
    FATP_RUN_TEST_NS(runner, slotmap, get_unchecked_performance_pattern);

    // Stress tests
    FATP_RUN_TEST_NS(runner, slotmap, stress_insert_erase);
    FATP_RUN_TEST_NS(runner, slotmap, stress_slot_reuse);

    // Critical bug fix tests
    FATP_RUN_TEST_NS(runner, slotmap, clear_aba_fix);
    FATP_RUN_TEST_NS(runner, slotmap, generation_wrap_skips_zero);

    // insert_at tests
    FATP_RUN_TEST_NS(runner, slotmap, insert_at_honours_hint_free_slot);
    FATP_RUN_TEST_NS(runner, slotmap, insert_at_honours_hint_erased_slot);
    FATP_RUN_TEST_NS(runner, slotmap, insert_at_fallback_when_occupied);
    FATP_RUN_TEST_NS(runner, slotmap, insert_at_extends_slot_array);
    FATP_RUN_TEST_NS(runner, slotmap, insert_at_intermediate_slots_reusable);
    FATP_RUN_TEST_NS(runner, slotmap, insert_at_aba_safety);
    FATP_RUN_TEST_NS(runner, slotmap, insert_at_variadic_args);
    FATP_RUN_TEST_NS(runner, slotmap, insert_at_snapshot_restore);

    // New API tests
    FATP_RUN_TEST_NS(runner, slotmap, at_throws_on_invalid);
    FATP_RUN_TEST_NS(runner, slotmap, contains_alias);
    FATP_RUN_TEST_NS(runner, slotmap, emplace_alias);
    FATP_RUN_TEST_NS(runner, slotmap, handle_hashable);
    FATP_RUN_TEST_NS(runner, slotmap, handle_ordering);
    FATP_RUN_TEST_NS(runner, slotmap, swap_member);
    FATP_RUN_TEST_NS(runner, slotmap, shrink_to_fit);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_SlotMap() ? 0 : 1;
}
#endif
