/**
 * @file test_IdGenerator.cpp
 * @brief Comprehensive unit tests for IdGenerator.h
 *
 * @details Complete test suite for:
 * - Sequential ID generation and recycling
 * - Random ID allocation
 * - Thread-safe concurrent generation
 * - StrongId integration
 * - Expected error handling
 * - Overflow detection
 * - RAII IdGuard functionality
 * - All policy combinations
 * 
 * Test Configuration:
 * - Processor: Intel Core i7-8850H @ 2.60GHz
 * - RAM: 32GB  
 * - C++ Standard: C++17
 * - Build Modes: Debug and Release
 *
 * @version 1.0
 * @author C++ Utilities Library
 * @date 2025
 */

#include <iostream>
#include <thread>
#include <vector>
#include <set>
#include <atomic>
#include <chrono>
#include <mutex>

#include "IdGenerator.h"
#include "StrongId.h"
#include "Expected.h"
#include "ConcurrencyPolicies.h"
#include "test_IdGenerator.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing {

// =============================================================================
// Test Helpers
// =============================================================================

struct TestStats {
    size_t generated_count = 0;
    size_t released_count = 0;
    size_t collisions = 0;
    std::chrono::nanoseconds total_time{0};
};

// =============================================================================
// I. Basic Functionality Tests
// =============================================================================

bool test_idgen_basic_sequential() {
    
    SimpleIdGenerator<uint64_t> gen(100);
    
    // Generate first ID
    auto id1 = gen.generate();
    SIMPLE_ASSERT(id1.has_value(), "First ID generation failed");
    SIMPLE_ASSERT(id1.value() == 100, "First ID should be base (100)");
    SIMPLE_ASSERT(gen.active_count() == 1, "Active count should be 1");
    
    // Generate second ID
    auto id2 = gen.generate();
    SIMPLE_ASSERT(id2.has_value(), "Second ID generation failed");
    SIMPLE_ASSERT(id2.value() == 101, "Second ID should be 101");
    SIMPLE_ASSERT(gen.active_count() == 2, "Active count should be 2");
    
    // Release first ID
    auto release_result = gen.release(id1.value());
    SIMPLE_ASSERT(release_result.has_value(), "ID release failed");
    SIMPLE_ASSERT(gen.active_count() == 1, "Active count should be 1 after release");
    SIMPLE_ASSERT(gen.recycled_count() == 1, "Recycled count should be 1");
    
    // Next generation should reuse recycled ID
    auto id3 = gen.generate();
    SIMPLE_ASSERT(id3.has_value(), "Recycled ID generation failed");
    SIMPLE_ASSERT(id3.value() == 100, "Should reuse first ID");
    SIMPLE_ASSERT(gen.recycled_count() == 0, "Recycled count should be 0");
    
    return true;
}

bool test_idgen_strong_id_integration() {
    
    using UserId = StrongId<uint64_t, struct UserTag>;
    IdGenerator<UserId> user_gen(1000);
    
    auto id1 = user_gen.generate();
    SIMPLE_ASSERT(id1.has_value(), "StrongId generation failed");
    SIMPLE_ASSERT(id1.value().get() == 1000, "StrongId value should be 1000");
    
    auto id2 = user_gen.generate();
    SIMPLE_ASSERT(id2.value().get() == 1001, "Second StrongId should be 1001");
    
    // Release and reuse
    auto release_result = user_gen.release(id1.value());
    SIMPLE_ASSERT(release_result.has_value(), "Release should succeed");
    
    auto id3 = user_gen.generate();
    SIMPLE_ASSERT(id3.value().get() == 1000, "Should reuse released StrongId");
    
    return true;
}

bool test_idgen_error_handling() {
    
    SimpleIdGenerator<uint8_t> small_gen(250);
    
    // Generate IDs until overflow
    std::vector<uint8_t> ids;
    for (int i = 0; i < 10; ++i) {
        auto id = small_gen.generate();
        if (!id) {
            SIMPLE_ASSERT(id.error() == IdError::Overflow, "Should get overflow error");
            break;
        }
        ids.push_back(id.value());
    }
    
    SIMPLE_ASSERT(ids.size() == 6, "Should generate 6 IDs (250-255)");
    
    // Try invalid release
    auto release_result = small_gen.release(200); // Not in use
    SIMPLE_ASSERT(!release_result.has_value(), "Should fail to release invalid ID");
    SIMPLE_ASSERT(release_result.error() == IdError::InvalidRelease, 
                  "Should get InvalidRelease error");
    
    return true;
}

// =============================================================================
// II. RAII IdGuard Tests
// =============================================================================

bool test_idgen_raii_guard() {
    
    SimpleIdGenerator<uint64_t> gen(1);
    
    {
        auto guard_result = gen.scoped_id();
        SIMPLE_ASSERT(guard_result.has_value(), "Scoped ID generation failed");
        
        auto& guard = guard_result.value();
        SIMPLE_ASSERT(guard.get() == 1, "Guard should hold ID 1");
        SIMPLE_ASSERT(gen.active_count() == 1, "Active count should be 1");
        
        // Guard goes out of scope here
    }
    
    SIMPLE_ASSERT(gen.active_count() == 0, "ID should be released after guard destroyed");
    SIMPLE_ASSERT(gen.recycled_count() == 1, "ID should be recycled");
    
    // Next generation should reuse
    auto next_id = gen.generate();
    SIMPLE_ASSERT(next_id.value() == 1, "Should reuse ID from guard");
    
    return true;
}

bool test_idgen_guard_move_semantics() {
    
    SimpleIdGenerator<uint64_t> gen(100);
    
    auto create_guard = [&gen]() {
        auto guard_result = gen.scoped_id();
        return std::move(guard_result.value());
    };
    
    {
        auto guard = create_guard();
        SIMPLE_ASSERT(guard.get() == 100, "Moved guard should hold ID");
        SIMPLE_ASSERT(gen.active_count() == 1, "Active count should be 1");
    }
    
    SIMPLE_ASSERT(gen.active_count() == 0, "ID released after moved guard destroyed");
    
    return true;
}

// =============================================================================
// III. Random Allocation Policy Tests
// =============================================================================

bool test_idgen_random_allocation() {
    
    RandomIdGenerator<uint64_t> random_gen;
    
    std::set<uint64_t> generated_ids;
    const size_t test_count = 100;
    
    for (size_t i = 0; i < test_count; ++i) {
        auto id = random_gen.generate();
        SIMPLE_ASSERT(id.has_value(), "Random ID generation failed");
        
        // Check for collisions (should be extremely rare with uint64_t)
        SIMPLE_ASSERT(generated_ids.find(id.value()) == generated_ids.end(),
                      "Random ID collision detected!");
        generated_ids.insert(id.value());
    }
    
    SIMPLE_ASSERT(generated_ids.size() == test_count,
                  "Should generate unique random IDs");
    
    return true;
}

// =============================================================================
// IV. Thread Safety Tests
// =============================================================================

bool test_idgen_thread_safety() {
    
    ThreadSafeIdGenerator<uint64_t> safe_gen(1);
    
    const size_t num_threads = 4;
    const size_t ids_per_thread = 1000;
    std::atomic<size_t> collision_count{0};
    std::vector<std::set<uint64_t>> thread_ids(num_threads);
    
    auto worker = [&](size_t thread_id) {
        for (size_t i = 0; i < ids_per_thread; ++i) {
            auto id = safe_gen.generate();
            if (id) {
                thread_ids[thread_id].insert(id.value());
            }
        }
    };
    
    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Check for collisions across threads
    std::set<uint64_t> all_ids;
    for (const auto& thread_set : thread_ids) {
        for (uint64_t id : thread_set) {
            SIMPLE_ASSERT(all_ids.find(id) == all_ids.end(),
                          "Thread-safety violation: duplicate ID");
            all_ids.insert(id);
        }
    }
    
    size_t total_expected = num_threads * ids_per_thread;
    SIMPLE_ASSERT(all_ids.size() == total_expected,
                  "Should generate correct number of unique IDs");
    
    std::cout << "    ✓ Generated " << all_ids.size() 
              << " unique IDs across " << num_threads 
              << " threads in " << duration.count() << " ms\n";
    
    return true;
}

// =============================================================================
// V. Recycling Policy Tests
// =============================================================================

bool test_idgen_no_recycling() {
    
    using NoRecycleGen = IdGenerator<uint64_t,
        SequentialAllocationPolicy<uint64_t>,
        NoRecyclingPolicy<uint64_t>>;
    
    NoRecycleGen gen(1);
    
    auto id1 = gen.generate();
    auto id2 = gen.generate();
    
    (void)gen.release(id1.value());
    SIMPLE_ASSERT(gen.recycled_count() == 0, "No recycling policy should not store IDs");
    
    auto id3 = gen.generate();
    SIMPLE_ASSERT(id3.value() == 3, "Should continue sequence without recycling");
    
    return true;
}

// =============================================================================
// VI. Performance Tests
// =============================================================================

bool test_idgen_performance() {
    
    SimpleIdGenerator<uint64_t> gen(1);
    
    const size_t iteration_count = 100000;
    
    // Benchmark generation
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<uint64_t> ids;
    ids.reserve(iteration_count);
    
    for (size_t i = 0; i < iteration_count; ++i) {
        auto id = gen.generate();
        if (id) {
            ids.push_back(id.value());
        }
    }
    
    auto mid = std::chrono::high_resolution_clock::now();
    
    // Benchmark release
    for (uint64_t id : ids) {
        (void)gen.release(id);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    
    auto gen_time = std::chrono::duration_cast<std::chrono::nanoseconds>(mid - start);
    auto rel_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - mid);
    
    double gen_ns_per_op = static_cast<double>(gen_time.count()) / iteration_count;
    double rel_ns_per_op = static_cast<double>(rel_time.count()) / iteration_count;
    
    std::cout << "    ✓ Generate: " << gen_ns_per_op << " ns/op\n";
    std::cout << "    ✓ Release:  " << rel_ns_per_op << " ns/op\n";
    
    // Benchmark recycling (should be faster than fresh generation)
    std::vector<uint64_t> recycled_ids;
    recycled_ids.reserve(iteration_count);
    
    start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < iteration_count; ++i) {
        auto id = gen.generate(); // All from recycled pool
        if (id) {
            recycled_ids.push_back(id.value());
        }
    }
    
    end = std::chrono::high_resolution_clock::now();
    
    auto recycle_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    double recycle_ns_per_op = static_cast<double>(recycle_time.count()) / iteration_count;
    
    std::cout << "    ✓ Recycled generate: " << recycle_ns_per_op << " ns/op\n";
    SIMPLE_ASSERT(recycle_ns_per_op <= gen_ns_per_op * 1.5,
                  "Recycled generation should be comparable to fresh");
    
    return true;
}

// =============================================================================
// VII. Edge Cases
// =============================================================================

bool test_idgen_edge_cases() {
    
    // Test with base_id at 0
    SimpleIdGenerator<uint64_t> gen_zero(0);
    auto id_zero = gen_zero.generate();
    SIMPLE_ASSERT(id_zero.value() == 0, "Should handle base_id of 0");
    
    // Test reset functionality
    SimpleIdGenerator<uint64_t> gen(100);
    (void)gen.generate();
    (void)gen.generate();
    SIMPLE_ASSERT(gen.active_count() == 2, "Should have 2 active IDs");
    
    gen.reset();
    SIMPLE_ASSERT(gen.active_count() == 0, "Reset should clear active IDs");
    SIMPLE_ASSERT(gen.recycled_count() == 0, "Reset should clear recycled IDs");
    
    auto id_after_reset = gen.generate();
    SIMPLE_ASSERT(id_after_reset.value() == 100, "Should restart from base after reset");
    
    // Test is_active query
    SIMPLE_ASSERT(gen.is_active(id_after_reset.value()), "Should report ID as active");
    (void)gen.release(id_after_reset.value());
    SIMPLE_ASSERT(!gen.is_active(id_after_reset.value()), "Should report ID as inactive");
    
    return true;
}

// =============================================================================
// Main Test Runner
// =============================================================================

bool test_IdGenerator() {

    PRINT_HEADER(ID GENERATOR)

    TestRunner runner;

    RUN_TEST(runner, idgen_basic_sequential);
    RUN_TEST(runner, idgen_strong_id_integration);
    RUN_TEST(runner, idgen_error_handling);
    RUN_TEST(runner, idgen_raii_guard);
    RUN_TEST(runner, idgen_guard_move_semantics);
    RUN_TEST(runner, idgen_random_allocation);
    RUN_TEST(runner, idgen_no_recycling);
    RUN_TEST(runner, idgen_thread_safety);
    RUN_TEST(runner, idgen_performance);
    RUN_TEST(runner, idgen_edge_cases);

    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing
