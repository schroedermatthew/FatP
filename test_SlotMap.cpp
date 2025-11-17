#include <iostream>
#include <string>
#include <vector>

#include "SlotMap.h"
#include "FatPTest.h"

#include "test_SlotMap.h"

namespace fat_p::testing
{

// Test data structure
struct Entity {
    int id;
    std::string name;
    float health;
    
    Entity(int i = 0, std::string n = "", float h = 0) 
        : id(i), name(std::move(n)), health(h) {}
};

bool test_slot_map_basic_insert_get() {
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

bool test_slot_map_multiple_inserts() {
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

bool test_slot_map_erase() {
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

bool test_slot_map_generational_safety() {
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
    
    return true;
}

bool test_slot_map_iteration() {
    SlotMap<Entity> map;
    
    map.insert(Entity{1, "Alice", 100.0f});
    map.insert(Entity{2, "Bob", 80.0f});
    map.insert(Entity{3, "Charlie", 90.0f});
    
    int count = 0;
    int sum_ids = 0;
    
    for (const auto& entity : map) {
        ++count;
        sum_ids += entity.id;
    }
    
    SIMPLE_ASSERT(count == 3, "Should iterate over 3 entities");
    SIMPLE_ASSERT(sum_ids == 6, "Sum of IDs should be 6");
    
    return true;
}

bool test_slot_map_iteration_after_erase() {
    SlotMap<Entity> map;
    
    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map.insert(Entity{2, "Bob", 80.0f});
    auto h3 = map.insert(Entity{3, "Charlie", 90.0f});
    
    // Erase middle element
    map.erase(h2);
    
    std::vector<int> ids;
    for (const auto& entity : map) {
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

bool test_slot_map_clear() {
    SlotMap<Entity> map;
    
    map.insert(Entity{1, "Alice", 100.0f});
    map.insert(Entity{2, "Bob", 80.0f});
    map.insert(Entity{3, "Charlie", 90.0f});
    
    SIMPLE_ASSERT(map.size() == 3, "Map should have 3 elements");
    
    map.clear();
    
    SIMPLE_ASSERT(map.empty(), "Map should be empty after clear");
    SIMPLE_ASSERT(map.size() == 0, "Map should have size 0 after clear");
    
    return true;
}

bool test_slot_map_handle_equality() {
    SlotMap<Entity> map;
    
    auto h1 = map.insert(Entity{1, "Alice", 100.0f});
    auto h2 = map.insert(Entity{2, "Bob", 80.0f});
    
    SIMPLE_ASSERT(h1 == h1, "Handle should equal itself");
    SIMPLE_ASSERT(h1 != h2, "Different handles should not be equal");
    
    auto h1_copy = h1;
    SIMPLE_ASSERT(h1 == h1_copy, "Copied handle should be equal");
    
    return true;
}

bool test_slot_map_reserve() {
    SlotMap<Entity> map;
    
    map.reserve(100);
    SIMPLE_ASSERT(map.capacity() >= 100, "Capacity should be at least 100");
    SIMPLE_ASSERT(map.empty(), "Map should still be empty after reserve");
    
    return true;
}

bool test_slot_map_stress_insert_erase() {
    SlotMap<int> map;
    std::vector<SlotMap<int>::Handle> handles;
    
    // Insert many elements
    for (int i = 0; i < 1000; ++i) {
        handles.push_back(map.insert(i));
    }
    
    SIMPLE_ASSERT(map.size() == 1000, "Map should have 1000 elements");
    
    // Erase every other element
    for (size_t i = 0; i < handles.size(); i += 2) {
        map.erase(handles[i]);
    }
    
    SIMPLE_ASSERT(map.size() == 500, "Map should have 500 elements after erasure");
    
    // Verify remaining elements
    for (size_t i = 1; i < handles.size(); i += 2) {
        int* val = map.get(handles[i]);
        SIMPLE_ASSERT(val != nullptr, "Handle should be valid");
        SIMPLE_ASSERT(*val == static_cast<int>(i), "Value should match");
    }
    
    // Verify erased elements are invalid
    for (size_t i = 0; i < handles.size(); i += 2) {
        SIMPLE_ASSERT(map.get(handles[i]) == nullptr, "Erased handle should be invalid");
    }
    
    return true;
}

void benchmark_slot_map() {
    std::cout << "\n" << colors::cyan() << "SlotMap Benchmarks:" << colors::reset() << "\n\n";
    
    SlotMap<Entity> map;
    map.reserve(10000);
    
    // Benchmark insert
    double insert_time = measure_perf([&map, i=0]() mutable {
        map.insert(Entity{i++, "Entity", 100.0f});
    }, 10000, 100);
    std::cout << "Insert: " << format_time(insert_time) << "\n";
    
    // Create handles for access benchmarks
    std::vector<SlotMap<Entity>::Handle> handles;
    handles.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        handles.push_back(map.insert(Entity{i, "Entity", 100.0f}));
    }
    
    // Benchmark get
    double get_time = measure_perf([&map, &handles, i=0]() mutable {
        Entity* e = map.get(handles[i % handles.size()]);
        DoNotOptimize(e);
        ++i;
    }, 100000, 1000);
    std::cout << "Get: " << format_time(get_time) << "\n";
    
    // Benchmark erase
    SlotMap<Entity> map2;
    std::vector<SlotMap<Entity>::Handle> erase_handles;
    for (int i = 0; i < 10000; ++i) {
        erase_handles.push_back(map2.insert(Entity{i, "Entity", 100.0f}));
    }
    
    double erase_time = measure_perf([&map2, &erase_handles, i=0]() mutable {
        if (i < erase_handles.size()) {
            map2.erase(erase_handles[i]);
        }
        ++i;
    }, 10000, 0);
    std::cout << "Erase: " << format_time(erase_time) << "\n";
    
    // Benchmark iteration
    SlotMap<int> map3;
    for (int i = 0; i < 10000; ++i) {
        map3.insert(i);
    }
    
    double iter_time = measure_perf([&map3]() {
        int sum = 0;
        for (int val : map3) {
            sum += val;
        }
        DoNotOptimize(sum);
    }, 1000, 10);
    std::cout << "Iteration (10k elements): " << format_time(iter_time) << "\n";
}

bool test_SlotMap() {

    PRINT_HEADER(SLOT MAP)

    TestRunner runner;

    RUN_TEST(runner, slot_map_basic_insert_get);
    RUN_TEST(runner, slot_map_multiple_inserts);
    RUN_TEST(runner, slot_map_erase);
    RUN_TEST(runner, slot_map_generational_safety);
    RUN_TEST(runner, slot_map_iteration);
    RUN_TEST(runner, slot_map_iteration_after_erase);
    RUN_TEST(runner, slot_map_clear);
    RUN_TEST(runner, slot_map_handle_equality);
    RUN_TEST(runner, slot_map_reserve);
    RUN_TEST(runner, slot_map_stress_insert_erase);

    benchmark_slot_map();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing
