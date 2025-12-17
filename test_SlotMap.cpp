#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include "SlotMap.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_SlotMap.h"
#endif

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

TEST_CASE(basic_insert_get)
{
    SlotMap<Entity> map;

    SIMPLE_ASSERT(map.empty(), "Map should start empty");
    SIMPLE_ASSERT(map.size() == 0, "Map should have size 0");

    auto handle = map.insert(Entity{1, "Alice", 100.0f});

    SIMPLE_ASSERT(!map.empty(), "Map should not be empty");
    SIMPLE_ASSERT(map.size() == 1, "Map should have size 1");
    SIMPLE_ASSERT(map.is_valid(handle), "Handle should be valid");

    Entity* entity = map.get(handle);
    SIMPLE_ASSERT(entity != nullptr, "Should get valid pointer");
    SIMPLE_ASSERT(entity->id == 1, "ID should match");
    SIMPLE_ASSERT(entity->name == "Alice", "Name should match");
    SIMPLE_ASSERT(entity->health == 100.0f, "Health should match");

    return true;
}

TEST_CASE(multiple_inserts)
{
    SlotMap<Entity> map;

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map.insert(Entity{2, "Bob", 80.0f});
    auto h3 = map.insert(Entity{3, "Charlie", 90.0f});

    SIMPLE_ASSERT(map.size() == 3, "Map should have 3 elements");

    SIMPLE_ASSERT(map.get(h1)->id == 1, "First entity ID should be 1");
    SIMPLE_ASSERT(map.get(h2)->id == 2, "Second entity ID should be 2");
    SIMPLE_ASSERT(map.get(h3)->id == 3, "Third entity ID should be 3");

    return true;
}

TEST_CASE(in_place_construction)
{
    SlotMap<Entity> map;

    // Insert using in-place construction (forwarding arguments)
    auto handle = map.insert(42, "InPlace", 75.0f);

    Entity* entity = map.get(handle);
    SIMPLE_ASSERT(entity != nullptr, "Should get valid pointer");
    SIMPLE_ASSERT(entity->id == 42, "ID should be 42");
    SIMPLE_ASSERT(entity->name == "InPlace", "Name should match");
    SIMPLE_ASSERT(entity->health == 75.0f, "Health should match");

    return true;
}

TEST_CASE(erase)
{
    SlotMap<Entity> map;

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map.insert(Entity{2, "Bob", 80.0f});
    auto h3 = map.insert(Entity{3, "Charlie", 90.0f});

    SIMPLE_ASSERT(map.size() == 3, "Map should have 3 elements");

    bool erased = map.erase(h2);
    SIMPLE_ASSERT(erased, "Erase should succeed");
    SIMPLE_ASSERT(map.size() == 2, "Map should have 2 elements after erase");

    SIMPLE_ASSERT(map.get(h2) == nullptr, "Erased handle should return nullptr");
    SIMPLE_ASSERT(!map.is_valid(h2), "Erased handle should be invalid");

    SIMPLE_ASSERT(map.get(h1) != nullptr, "Other handles should still be valid");
    SIMPLE_ASSERT(map.get(h3) != nullptr, "Other handles should still be valid");

    return true;
}

TEST_CASE(erase_invalid_handle)
{
    SlotMap<Entity> map;

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});

    // Erase once (valid)
    SIMPLE_ASSERT(map.erase(h1) == true, "First erase should succeed");

    // Erase again (invalid - already erased)
    SIMPLE_ASSERT(map.erase(h1) == false, "Second erase should fail");

    // Erase default handle
    SlotMap<Entity>::Handle null_handle;
    SIMPLE_ASSERT(map.erase(null_handle) == false, "Erasing null handle should fail");

    return true;
}

TEST_CASE(clear)
{
    SlotMap<Entity> map;

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map.insert(Entity{2, "Bob", 80.0f});
    auto h3 = map.insert(Entity{3, "Charlie", 90.0f});

    SIMPLE_ASSERT(map.size() == 3, "Map should have 3 elements");

    map.clear();

    SIMPLE_ASSERT(map.empty(), "Map should be empty after clear");
    SIMPLE_ASSERT(map.size() == 0, "Map should have size 0 after clear");

    // Old handles should be invalid
    SIMPLE_ASSERT(!map.is_valid(h1), "Handle should be invalid after clear");
    SIMPLE_ASSERT(!map.is_valid(h2), "Handle should be invalid after clear");
    SIMPLE_ASSERT(!map.is_valid(h3), "Handle should be invalid after clear");

    return true;
}

// =============================================================================
// Generational Safety (ABA Problem)
// =============================================================================

TEST_CASE(generational_safety)
{
    SlotMap<Entity> map;

    // Insert and get handle
    auto handle1 = map.insert(Entity{1, "Alice", 100.0f});
    SIMPLE_ASSERT(map.is_valid(handle1), "Handle should be valid");

    // Erase
    map.erase(handle1);
    SIMPLE_ASSERT(!map.is_valid(handle1), "Handle should be invalid after erase");
    SIMPLE_ASSERT(map.get(handle1) == nullptr, "Should return nullptr for invalid handle");

    // Insert new entity (reuses slot with new generation)
    auto handle2 = map.insert(Entity{2, "Bob", 80.0f});

    // Old handle should still be invalid (different generation)
    SIMPLE_ASSERT(!map.is_valid(handle1), "Old handle should still be invalid");
    SIMPLE_ASSERT(map.get(handle1) == nullptr, "Old handle should return nullptr");

    // New handle should be valid
    SIMPLE_ASSERT(map.is_valid(handle2), "New handle should be valid");
    SIMPLE_ASSERT(map.get(handle2)->id == 2, "New handle should access correct entity");

    // Verify handles have same index but different generation
    SIMPLE_ASSERT(handle1.index == handle2.index, "Slot should be reused");
    SIMPLE_ASSERT(handle1.generation != handle2.generation, "Generation should differ");

    return true;
}

TEST_CASE(aba_multiple_cycles)
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
        SIMPLE_ASSERT(!map.is_valid(old_handles[i]), "Old handle should be invalid");
        SIMPLE_ASSERT(map.get(old_handles[i]) == nullptr, "Old handle should return nullptr");
    }

    // Final handle should be valid
    SIMPLE_ASSERT(map.is_valid(final_handle), "Final handle should be valid");
    SIMPLE_ASSERT(*map.get(final_handle) == 999, "Final value should be 999");

    return true;
}

// =============================================================================
// Handle Tests
// =============================================================================

TEST_CASE(handle_default_state)
{
    SlotMap<Entity>::Handle handle;

    SIMPLE_ASSERT(handle.is_null(), "Default handle should be null");
    SIMPLE_ASSERT(!static_cast<bool>(handle), "Default handle should be falsy");
    SIMPLE_ASSERT(handle.index == 0, "Default index should be 0");
    SIMPLE_ASSERT(handle.generation == 0, "Default generation should be 0");

    return true;
}

TEST_CASE(handle_equality)
{
    SlotMap<Entity> map;

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map.insert(Entity{2, "Bob", 80.0f});

    SIMPLE_ASSERT(h1 == h1, "Handle should equal itself");
    SIMPLE_ASSERT(h1 != h2, "Different handles should not be equal");

    auto h1_copy = h1;
    SIMPLE_ASSERT(h1 == h1_copy, "Copied handle should be equal");

    // Handles with same index but different generation should not be equal
    map.erase(h1);
    auto h3 = map.insert(Entity{3, "Charlie", 90.0f});
    SIMPLE_ASSERT(h1.index == h3.index, "Should reuse same slot");
    SIMPLE_ASSERT(h1 != h3, "Different generations should not be equal");

    return true;
}

TEST_CASE(handle_is_null_vs_is_valid)
{
    SlotMap<Entity> map;

    // Default handle: is_null() = true, is_valid() = false
    SlotMap<Entity>::Handle null_handle;
    SIMPLE_ASSERT(null_handle.is_null(), "Default handle is_null() should be true");
    SIMPLE_ASSERT(!map.is_valid(null_handle), "Default handle is_valid() should be false");

    // Valid handle: is_null() = false, is_valid() = true
    auto valid_handle = map.insert(Entity{1, "Test", 100.0f});
    SIMPLE_ASSERT(!valid_handle.is_null(), "Valid handle is_null() should be false");
    SIMPLE_ASSERT(map.is_valid(valid_handle), "Valid handle is_valid() should be true");

    // Erased handle: is_null() = false, is_valid() = false
    map.erase(valid_handle);
    SIMPLE_ASSERT(!valid_handle.is_null(), "Erased handle is_null() should be false");
    SIMPLE_ASSERT(!map.is_valid(valid_handle), "Erased handle is_valid() should be false");

    return true;
}

TEST_CASE(handle_operator_bool)
{
    SlotMap<Entity>::Handle null_handle;
    SIMPLE_ASSERT(!null_handle, "Null handle should be falsy");

    SlotMap<Entity> map;
    auto handle = map.insert(Entity{1, "Test", 100.0f});
    SIMPLE_ASSERT(static_cast<bool>(handle), "Non-null handle should be truthy");

    // Note: operator bool only checks is_null(), not is_valid()
    map.erase(handle);
    SIMPLE_ASSERT(static_cast<bool>(handle), "Erased handle is still truthy (not null)");

    return true;
}

// =============================================================================
// Iteration Tests
// =============================================================================

TEST_CASE(iteration)
{
    SlotMap<Entity> map;

    map.insert(Entity{1, "Alice", 100.0f});
    map.insert(Entity{2, "Bob", 80.0f});
    map.insert(Entity{3, "Charlie", 90.0f});

    int count = 0;
    int sum_ids = 0;

    for (const auto& entity : map)
    {
        ++count;
        sum_ids += entity.id;
    }

    SIMPLE_ASSERT(count == 3, "Should iterate over 3 entities");
    SIMPLE_ASSERT(sum_ids == 6, "Sum of IDs should be 6");

    return true;
}

TEST_CASE(iteration_empty)
{
    SlotMap<Entity> map;

    int count = 0;
    for (const auto& entity : map)
    {
        (void)entity;
        ++count;
    }

    SIMPLE_ASSERT(count == 0, "Empty map iteration should have 0 elements");

    return true;
}

TEST_CASE(iteration_after_erase)
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

    SIMPLE_ASSERT(ids.size() == 2, "Should iterate over 2 entities");

    // Should have entities 1 and 3 (order may vary due to swap-and-pop)
    bool has_1 = std::find(ids.begin(), ids.end(), 1) != ids.end();
    bool has_3 = std::find(ids.begin(), ids.end(), 3) != ids.end();
    bool has_2 = std::find(ids.begin(), ids.end(), 2) != ids.end();

    SIMPLE_ASSERT(has_1, "Should have entity 1");
    SIMPLE_ASSERT(has_3, "Should have entity 3");
    SIMPLE_ASSERT(!has_2, "Should not have entity 2");

    return true;
}

TEST_CASE(const_iteration)
{
    SlotMap<Entity> map;

    map.insert(Entity{1, "Alice", 100.0f});
    map.insert(Entity{2, "Bob", 80.0f});

    const SlotMap<Entity>& const_map = map;

    int count = 0;
    for (const auto& entity : const_map)
    {
        (void)entity;
        ++count;
    }

    SIMPLE_ASSERT(count == 2, "Const iteration should work");

    // Test cbegin/cend
    count = 0;
    for (auto it = map.cbegin(); it != map.cend(); ++it)
    {
        ++count;
    }

    SIMPLE_ASSERT(count == 2, "cbegin/cend should work");

    return true;
}

TEST_CASE(entries_iteration)
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

    SIMPLE_ASSERT(handles.size() == 3, "Should have 3 entries");
    SIMPLE_ASSERT(ids.size() == 3, "Should have 3 IDs");

    // All handles should be valid
    for (const auto& h : handles)
    {
        SIMPLE_ASSERT(map.is_valid(h), "Handle from entries() should be valid");
    }

    // Should contain all our original handles
    bool found_h1 = std::find(handles.begin(), handles.end(), h1) != handles.end();
    bool found_h2 = std::find(handles.begin(), handles.end(), h2) != handles.end();
    bool found_h3 = std::find(handles.begin(), handles.end(), h3) != handles.end();

    SIMPLE_ASSERT(found_h1, "Should find handle 1");
    SIMPLE_ASSERT(found_h2, "Should find handle 2");
    SIMPLE_ASSERT(found_h3, "Should find handle 3");

    return true;
}

TEST_CASE(entries_const_iteration)
{
    SlotMap<Entity> map;

    map.insert(Entity{1, "Alice", 100.0f});
    map.insert(Entity{2, "Bob", 80.0f});

    const SlotMap<Entity>& const_map = map;

    int count = 0;
    for (auto entry : const_map.entries())
    {
        SIMPLE_ASSERT(const_map.is_valid(entry.handle), "Handle should be valid");
        ++count;
    }

    SIMPLE_ASSERT(count == 2, "Const entries iteration should work");

    return true;
}

TEST_CASE(entries_empty)
{
    SlotMap<Entity> map;

    int count = 0;
    for (auto entry : map.entries())
    {
        (void)entry;
        ++count;
    }

    SIMPLE_ASSERT(count == 0, "Empty entries iteration should have 0 elements");

    return true;
}

TEST_CASE(entries_modification)
{
    SlotMap<Entity> map;

    map.insert(Entity{1, "Alice", 100.0f});
    map.insert(Entity{2, "Bob", 80.0f});

    // Modify values through entries
    for (auto entry : map.entries())
    {
        entry.value.health += 10.0f;
    }

    // Verify modification
    for (const auto& entity : map)
    {
        SIMPLE_ASSERT(entity.health == 110.0f || entity.health == 90.0f,
                      "Health should be modified");
    }

    return true;
}

// =============================================================================
// Copy/Move Tests
// =============================================================================

TEST_CASE(copy_construction)
{
    SlotMap<Entity> map1;

    auto h1 = map1.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map1.insert(Entity{2, "Bob", 80.0f});

    SlotMap<Entity> map2(map1);

    // Both maps should have same size
    SIMPLE_ASSERT(map2.size() == 2, "Copied map should have 2 elements");

    // Handles from original should work in copy
    SIMPLE_ASSERT(map2.is_valid(h1), "Handle should be valid in copy");
    SIMPLE_ASSERT(map2.is_valid(h2), "Handle should be valid in copy");

    SIMPLE_ASSERT(map2.get(h1)->id == 1, "Data should be copied correctly");
    SIMPLE_ASSERT(map2.get(h2)->id == 2, "Data should be copied correctly");

    // Modifying copy should not affect original
    map2.get(h1)->health = 50.0f;
    SIMPLE_ASSERT(map1.get(h1)->health == 100.0f, "Original should be unchanged");
    SIMPLE_ASSERT(map2.get(h1)->health == 50.0f, "Copy should be modified");

    return true;
}

TEST_CASE(copy_assignment)
{
    SlotMap<Entity> map1;
    auto h1 = map1.insert(Entity{1, "Alice", 100.0f});

    SlotMap<Entity> map2;
    map2.insert(Entity{99, "Temp", 0.0f});

    map2 = map1;

    SIMPLE_ASSERT(map2.size() == 1, "Assigned map should have 1 element");
    SIMPLE_ASSERT(map2.is_valid(h1), "Handle should be valid after assignment");
    SIMPLE_ASSERT(map2.get(h1)->id == 1, "Data should be copied correctly");

    return true;
}

TEST_CASE(move_construction)
{
    SlotMap<Entity> map1;
    auto h1 = map1.insert(Entity{1, "Alice", 100.0f});

    SlotMap<Entity> map2(std::move(map1));

    SIMPLE_ASSERT(map2.size() == 1, "Moved-to map should have 1 element");
    SIMPLE_ASSERT(map2.is_valid(h1), "Handle should be valid in moved-to map");
    SIMPLE_ASSERT(map2.get(h1)->id == 1, "Data should be moved correctly");

    // Original should be empty (moved-from state)
    SIMPLE_ASSERT(map1.empty(), "Moved-from map should be empty");

    return true;
}

TEST_CASE(move_assignment)
{
    SlotMap<Entity> map1;
    auto h1 = map1.insert(Entity{1, "Alice", 100.0f});

    SlotMap<Entity> map2;
    map2.insert(Entity{99, "Temp", 0.0f});

    map2 = std::move(map1);

    SIMPLE_ASSERT(map2.size() == 1, "Moved-to map should have 1 element");
    SIMPLE_ASSERT(map2.is_valid(h1), "Handle should be valid after move assignment");
    SIMPLE_ASSERT(map2.get(h1)->id == 1, "Data should be moved correctly");

    return true;
}

// =============================================================================
// Capacity Tests
// =============================================================================

TEST_CASE(reserve)
{
    SlotMap<Entity> map;

    map.reserve(100);
    SIMPLE_ASSERT(map.capacity() >= 100, "Capacity should be at least 100");
    SIMPLE_ASSERT(map.empty(), "Map should still be empty after reserve");

    return true;
}

TEST_CASE(slot_count_tracking)
{
    SlotMap<Entity> map;

    SIMPLE_ASSERT(map.slot_count() == 0, "Initial slot count should be 0");
    SIMPLE_ASSERT(map.free_slot_count() == 0, "Initial free slot count should be 0");

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    (void)map.insert(Entity{2, "Bob", 80.0f});

    SIMPLE_ASSERT(map.slot_count() == 2, "Should have 2 slots");
    SIMPLE_ASSERT(map.free_slot_count() == 0, "Should have 0 free slots");

    map.erase(h1);

    SIMPLE_ASSERT(map.slot_count() == 2, "Slot count unchanged after erase");
    SIMPLE_ASSERT(map.free_slot_count() == 1, "Should have 1 free slot");

    // Insert should reuse the free slot
    auto h3 = map.insert(Entity{3, "Charlie", 90.0f});
    (void)h3;

    SIMPLE_ASSERT(map.slot_count() == 2, "Slot count should still be 2 (reused)");
    SIMPLE_ASSERT(map.free_slot_count() == 0, "Should have 0 free slots");

    return true;
}

// =============================================================================
// Unchecked Access Tests
// =============================================================================

TEST_CASE(get_unchecked_basic)
{
    SlotMap<Entity> map;

    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map.insert(Entity{2, "Bob", 80.0f});

    // get_unchecked returns reference, not pointer
    Entity& e1 = map.get_unchecked(h1);
    SIMPLE_ASSERT(e1.id == 1, "Should access correct entity");
    SIMPLE_ASSERT(e1.name == "Alice", "Name should match");

    // Modify through unchecked access
    e1.health = 50.0f;
    SIMPLE_ASSERT(map.get(h1)->health == 50.0f, "Modification should persist");

    // Const version
    const SlotMap<Entity>& const_map = map;
    const Entity& e2 = const_map.get_unchecked(h2);
    SIMPLE_ASSERT(e2.id == 2, "Const access should work");

    return true;
}

TEST_CASE(get_unchecked_performance_pattern)
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

    SIMPLE_ASSERT(sum == 49500, "Sum should be 0+10+20+...+990 = 49500");

    return true;
}

// =============================================================================
// Stress Tests
// =============================================================================

TEST_CASE(stress_insert_erase)
{
    SlotMap<int> map;
    std::vector<SlotMap<int>::Handle> handles;

    // Insert many elements
    for (int i = 0; i < 1000; ++i)
    {
        handles.push_back(map.insert(i));
    }

    SIMPLE_ASSERT(map.size() == 1000, "Map should have 1000 elements");

    // Erase every other element
    for (size_t i = 0; i < handles.size(); i += 2)
    {
        map.erase(handles[i]);
    }

    SIMPLE_ASSERT(map.size() == 500, "Map should have 500 elements after erasure");

    // Verify remaining elements
    for (size_t i = 1; i < handles.size(); i += 2)
    {
        int* val = map.get(handles[i]);
        SIMPLE_ASSERT(val != nullptr, "Handle should be valid");
        SIMPLE_ASSERT(*val == static_cast<int>(i), "Value should match");
    }

    // Verify erased elements are invalid
    for (size_t i = 0; i < handles.size(); i += 2)
    {
        SIMPLE_ASSERT(map.get(handles[i]) == nullptr, "Erased handle should be invalid");
    }

    return true;
}

TEST_CASE(stress_slot_reuse)
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

    SIMPLE_ASSERT(map.empty(), "Map should be empty");
    // Slot count should be 10 (all reused)
    SIMPLE_ASSERT(map.slot_count() == 10, "Only 10 slots should ever be allocated");

    return true;
}

// =============================================================================
// Benchmarks
// =============================================================================

void benchmark_slot_map()
{
    std::cout << "\n" << colors::cyan() << "SlotMap Benchmarks:" << colors::reset() << "\n\n";

    SlotMap<Entity> map;
    map.reserve(10000);

    // Benchmark insert
    int insert_counter = 0;
    double insert_time = measure_perf(
        [&map, &insert_counter]() { map.insert(Entity{insert_counter++, "Entity", 100.0f}); },
        10000,
        100);
    std::cout << "Insert: " << format_time(insert_time) << "\n";

    // Create handles for access benchmarks
    map.clear();
    std::vector<SlotMap<Entity>::Handle> handles;
    handles.reserve(1000);
    for (int i = 0; i < 1000; ++i)
    {
        handles.push_back(map.insert(Entity{i, "Entity", 100.0f}));
    }

    // Benchmark get
    int get_counter = 0;
    double get_time = measure_perf(
        [&map, &handles, &get_counter]()
        {
            Entity* e = map.get(handles[get_counter % handles.size()]);
            DoNotOptimize(e);
            ++get_counter;
        },
        100000,
        1000);
    std::cout << "Get: " << format_time(get_time) << "\n";

    // Benchmark is_valid
    int valid_counter = 0;
    double valid_time = measure_perf(
        [&map, &handles, &valid_counter]()
        {
            bool v = map.is_valid(handles[valid_counter % handles.size()]);
            DoNotOptimize(v);
            ++valid_counter;
        },
        100000,
        1000);
    std::cout << "is_valid: " << format_time(valid_time) << "\n";

    // Benchmark erase
    SlotMap<Entity> map2;
    std::vector<SlotMap<Entity>::Handle> erase_handles;
    for (int i = 0; i < 10000; ++i)
    {
        erase_handles.push_back(map2.insert(Entity{i, "Entity", 100.0f}));
    }

    int erase_counter = 0;
    double erase_time = measure_perf(
        [&map2, &erase_handles, &erase_counter]()
        {
            if (static_cast<size_t>(erase_counter) < erase_handles.size())
            {
                map2.erase(erase_handles[erase_counter]);
            }
            ++erase_counter;
        },
        10000,
        0);
    std::cout << "Erase: " << format_time(erase_time) << "\n";

    // Benchmark iteration
    SlotMap<int> map3;
    for (int i = 0; i < 10000; ++i)
    {
        map3.insert(i);
    }

    double iter_time = measure_perf(
        [&map3]()
        {
            int sum = 0;
            for (int val : map3)
            {
                sum += val;
            }
            DoNotOptimize(sum);
        },
        1000,
        10);
    std::cout << "Iteration (10k elements): " << format_time(iter_time) << "\n";

    // Benchmark entries iteration
    double entries_time = measure_perf(
        [&map3]()
        {
            int sum = 0;
            for (auto entry : map3.entries())
            {
                sum += entry.value;
            }
            DoNotOptimize(sum);
        },
        1000,
        10);
    std::cout << "Entries iteration (10k elements): " << format_time(entries_time) << "\n";

    // Benchmark get_unchecked vs get
    // Create handles for the int map
    std::vector<SlotMap<int>::Handle> int_handles;
    int_handles.reserve(1000);
    SlotMap<int> map4;
    for (int i = 0; i < 1000; ++i)
    {
        int_handles.push_back(map4.insert(i));
    }

    int unchecked_counter = 0;
    double unchecked_time = measure_perf(
        [&map4, &int_handles, &unchecked_counter]()
        {
            int& val = map4.get_unchecked(int_handles[unchecked_counter % int_handles.size()]);
            DoNotOptimize(val);
            ++unchecked_counter;
        },
        100000,
        1000);
    std::cout << "get_unchecked: " << format_time(unchecked_time) << "\n";
}

} // namespace fat_p::testing::slotmap

namespace fat_p::testing
{

bool test_SlotMap()
{
    PRINT_HEADER(SLOT MAP)

    TestRunner runner;

    // Basic operations
    RUN_TEST_NS(runner, slotmap, basic_insert_get);
    RUN_TEST_NS(runner, slotmap, multiple_inserts);
    RUN_TEST_NS(runner, slotmap, in_place_construction);
    RUN_TEST_NS(runner, slotmap, erase);
    RUN_TEST_NS(runner, slotmap, erase_invalid_handle);
    RUN_TEST_NS(runner, slotmap, clear);

    // Generational safety
    RUN_TEST_NS(runner, slotmap, generational_safety);
    RUN_TEST_NS(runner, slotmap, aba_multiple_cycles);

    // Handle tests
    RUN_TEST_NS(runner, slotmap, handle_default_state);
    RUN_TEST_NS(runner, slotmap, handle_equality);
    RUN_TEST_NS(runner, slotmap, handle_is_null_vs_is_valid);
    RUN_TEST_NS(runner, slotmap, handle_operator_bool);

    // Iteration tests
    RUN_TEST_NS(runner, slotmap, iteration);
    RUN_TEST_NS(runner, slotmap, iteration_empty);
    RUN_TEST_NS(runner, slotmap, iteration_after_erase);
    RUN_TEST_NS(runner, slotmap, const_iteration);
    RUN_TEST_NS(runner, slotmap, entries_iteration);
    RUN_TEST_NS(runner, slotmap, entries_const_iteration);
    RUN_TEST_NS(runner, slotmap, entries_empty);
    RUN_TEST_NS(runner, slotmap, entries_modification);

    // Copy/move tests
    RUN_TEST_NS(runner, slotmap, copy_construction);
    RUN_TEST_NS(runner, slotmap, copy_assignment);
    RUN_TEST_NS(runner, slotmap, move_construction);
    RUN_TEST_NS(runner, slotmap, move_assignment);

    // Capacity tests
    RUN_TEST_NS(runner, slotmap, reserve);
    RUN_TEST_NS(runner, slotmap, slot_count_tracking);

    // Unchecked access tests
    RUN_TEST_NS(runner, slotmap, get_unchecked_basic);
    RUN_TEST_NS(runner, slotmap, get_unchecked_performance_pattern);

    // Stress tests
    RUN_TEST_NS(runner, slotmap, stress_insert_erase);
    RUN_TEST_NS(runner, slotmap, stress_slot_reuse);

    slotmap::benchmark_slot_map();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_SlotMap() ? 0 : 1;
}
#endif
