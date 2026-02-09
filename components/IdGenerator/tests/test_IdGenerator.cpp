/**
 * @file test_IdGenerator.cpp
 * @brief Comprehensive unit tests for IdGenerator.h
 *
 * Test Configuration:
 * - Processor: Intel Core i7-8850H @ 2.60GHz
 * - RAM: 32GB
 * - C++ Standard: C++17
 * - Build Modes: Debug and Release
 *
 * @version 1.0
 */
/*
FATP_META:
  meta_version: 1
  component: IdGenerator
  file_role: test
  path: components/IdGenerator/tests/test_IdGenerator.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for IdGenerator."
  api_stability: in_work
  related:
    docs_search: "IdGenerator"
    headers:
      - include/fat_p/ConcurrencyPolicies.h
      - include/fat_p/Expected.h
      - include/fat_p/FatPTest.h
      - include/fat_p/IdGenerator.h
      - include/fat_p/StrongId.h
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

#include <atomic>
#include <chrono>
#include <iostream>
#include <set>
#include <thread>
#include <vector>

#include "ConcurrencyPolicies.h"
#include "Expected.h"
#include "FatPTest.h"
#include "IdGenerator.h"
#include "StrongId.h"

namespace fat_p::testing::idgenerator
{

// =============================================================================
// I. Basic Functionality Tests
// =============================================================================

FATP_TEST_CASE(basic_sequential)
{
    SimpleIdGenerator<uint64_t> gen(100);

    // Generate first ID
    auto id1 = gen.generate();
    FATP_ASSERT_TRUE(id1.has_value(), "First ID generation failed");
    FATP_ASSERT_EQ(id1.value(), uint64_t(100), "First ID should be base (100)");
    FATP_ASSERT_EQ(gen.active_count(), size_t(1), "Active count should be 1");

    // Generate second ID
    auto id2 = gen.generate();
    FATP_ASSERT_TRUE(id2.has_value(), "Second ID generation failed");
    FATP_ASSERT_EQ(id2.value(), uint64_t(101), "Second ID should be 101");
    FATP_ASSERT_EQ(gen.active_count(), size_t(2), "Active count should be 2");

    // Release first ID
    auto release_result = gen.release(id1.value());
    FATP_ASSERT_TRUE(release_result.has_value(), "ID release failed");
    FATP_ASSERT_EQ(gen.active_count(), size_t(1), "Active count should be 1 after release");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(1), "Recycled count should be 1");

    // Next generation should reuse recycled ID
    auto id3 = gen.generate();
    FATP_ASSERT_TRUE(id3.has_value(), "Recycled ID generation failed");
    FATP_ASSERT_EQ(id3.value(), uint64_t(100), "Should reuse first ID");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(0), "Recycled count should be 0");

    return true;
}

FATP_TEST_CASE(strong_id_integration)
{
    using UserId = StrongId<uint64_t, struct UserTag>;
    IdGenerator<UserId> user_gen(1000);

    auto id1 = user_gen.generate();
    FATP_ASSERT_TRUE(id1.has_value(), "StrongId generation failed");
    FATP_ASSERT_EQ(id1.value().get(), uint64_t(1000), "StrongId value should be 1000");

    auto id2 = user_gen.generate();
    FATP_ASSERT_EQ(id2.value().get(), uint64_t(1001), "Second StrongId should be 1001");

    // Release and reuse
    auto release_result = user_gen.release(id1.value());
    FATP_ASSERT_TRUE(release_result.has_value(), "Release should succeed");

    auto id3 = user_gen.generate();
    FATP_ASSERT_EQ(id3.value().get(), uint64_t(1000), "Should reuse released StrongId");

    return true;
}

FATP_TEST_CASE(error_handling)
{
    SimpleIdGenerator<uint8_t> small_gen(250);

    // Generate IDs until overflow
    std::vector<uint8_t> ids;
    for (int i = 0; i < 10; ++i)
    {
        auto id = small_gen.generate();
        if (!id.has_value())
        {
            FATP_ASSERT_TRUE(id.error() == IdError::Overflow, "Should get overflow error");
            break;
        }
        ids.push_back(id.value());
    }

    // Should generate exactly 6 IDs: 250, 251, 252, 253, 254, 255
    FATP_ASSERT_EQ(ids.size(), size_t(6), "Should generate 6 IDs (250-255)");

    // Try invalid release
    auto release_result = small_gen.release(200); // Not in use
    FATP_ASSERT_TRUE(!release_result.has_value(), "Should fail to release invalid ID");
    FATP_ASSERT_TRUE(release_result.error() == IdError::InvalidRelease, "Should get InvalidRelease error");

    return true;
}

// =============================================================================
// II. RAII IdGuard Tests
// =============================================================================

FATP_TEST_CASE(raii_guard)
{
    SimpleIdGenerator<uint64_t> gen(1);

    {
        auto guard_result = gen.scoped_id();
        FATP_ASSERT_TRUE(guard_result.has_value(), "Scoped ID generation failed");

        auto& guard = guard_result.value();
        FATP_ASSERT_EQ(guard.get(), uint64_t(1), "Guard should hold ID 1");
        FATP_ASSERT_EQ(gen.active_count(), size_t(1), "Active count should be 1");
    }

    FATP_ASSERT_EQ(gen.active_count(), size_t(0), "ID should be released after guard destroyed");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(1), "ID should be recycled");

    // Next generation should reuse
    auto next_id = gen.generate();
    FATP_ASSERT_EQ(next_id.value(), uint64_t(1), "Should reuse ID from guard");

    return true;
}

FATP_TEST_CASE(guard_move_semantics)
{
    SimpleIdGenerator<uint64_t> gen(100);

    auto create_guard = [&gen]() {
        auto guard_result = gen.scoped_id();
        return std::move(guard_result.value());
    };

    {
        auto guard = create_guard();
        FATP_ASSERT_EQ(guard.get(), uint64_t(100), "Moved guard should hold ID");
        FATP_ASSERT_EQ(gen.active_count(), size_t(1), "Active count should be 1");
    }

    FATP_ASSERT_EQ(gen.active_count(), size_t(0), "ID released after moved guard destroyed");

    return true;
}

FATP_TEST_CASE(guard_default_ctor)
{
    // Test IdGuard default constructor creates invalid guard
    SimpleIdGenerator<uint64_t> gen(1);
    using Guard = typename SimpleIdGenerator<uint64_t>::IdGuard;

    Guard default_guard;
    FATP_ASSERT_TRUE(!default_guard, "Default guard should be invalid (operator bool)");

    // Default guard destruction should be safe (no-op)
    // This is implicitly tested by the guard going out of scope

    // Move assignment from valid guard
    {
        auto scoped = gen.scoped_id();
        FATP_ASSERT_TRUE(scoped.has_value(), "scoped_id should succeed");

        default_guard = std::move(scoped.value());
        FATP_ASSERT_TRUE(static_cast<bool>(default_guard), "Guard should be valid after move");
        FATP_ASSERT_EQ(default_guard.get(), uint64_t(1), "Should hold ID 1");
    }

    FATP_ASSERT_EQ(gen.active_count(), size_t(1), "ID still active (guard holds it)");

    default_guard.release_ownership();
    FATP_ASSERT_TRUE(!default_guard, "Guard should be invalid after release_ownership");

    // ID is now leaked (intentionally) - verify it's still tracked
    FATP_ASSERT_EQ(gen.active_count(), size_t(1), "ID still tracked after ownership release");

    return true;
}

// =============================================================================
// III. Random Allocation Policy Tests
// =============================================================================

FATP_TEST_CASE(random_allocation)
{
    RandomIdGenerator<uint64_t> random_gen;

    std::set<uint64_t> generated_ids;
    const size_t test_count = 100;

    for (size_t i = 0; i < test_count; ++i)
    {
        auto id = random_gen.generate();
        FATP_ASSERT_TRUE(id.has_value(), "Random ID generation failed");

        // Check for collisions (should be extremely rare with uint64_t)
        FATP_ASSERT_TRUE(generated_ids.find(id.value()) == generated_ids.end(), "Random ID collision detected!");
        generated_ids.insert(id.value());
    }

    FATP_ASSERT_EQ(generated_ids.size(), test_count, "Should generate unique random IDs");

    return true;
}

FATP_TEST_CASE(random_small_type)
{
    // Test that RandomAllocationPolicy works correctly with uint8_t
    // This validates the fix for UB with std::uniform_int_distribution
    RandomIdGenerator<uint8_t> random_gen;

    std::set<uint8_t> generated_ids;
    size_t success_count = 0;
    [[maybe_unused]] size_t collision_error_count = 0;

    // Try to generate some IDs - collisions are expected with small type
    for (size_t i = 0; i < 50; ++i)
    {
        auto id = random_gen.generate();
        if (id.has_value())
        {
            // Verify generator doesn't return duplicate without error
            if (generated_ids.find(id.value()) != generated_ids.end())
            {
                // This would indicate a bug - generator returned dup without error
                FATP_ASSERT_TRUE(false, "Generator returned duplicate ID without error");
            }
            generated_ids.insert(id.value());
            ++success_count;
        }
        else
        {
            // Collision should return AlreadyInUse error
            FATP_ASSERT_TRUE(id.error() == IdError::AlreadyInUse, "Collision should return AlreadyInUse error");
            ++collision_error_count;
        }
    }

    // Should have generated at least some unique IDs
    FATP_ASSERT_TRUE(success_count > 0, "Should generate at least some unique IDs");

    return true;
}

FATP_TEST_CASE(random_seed_reproducibility)
{
    // Test that seeded random generator produces reproducible sequences
    constexpr uint64_t kTestSeed = 12345;

    // Manually construct policies with seed to test reproducibility
    RandomAllocationPolicy<uint64_t> policy1(kTestSeed, 0);
    RandomAllocationPolicy<uint64_t> policy2(kTestSeed, 0);

    // Generate sequence from both - should be identical
    std::vector<uint64_t> seq1, seq2;
    for (int i = 0; i < 10; ++i)
    {
        auto id1 = policy1.next_id(0, true);
        auto id2 = policy2.next_id(0, true);
        FATP_ASSERT_TRUE(id1.has_value() && id2.has_value(), "Generation should succeed");
        seq1.push_back(*id1);
        seq2.push_back(*id2);
    }

    // Sequences should be identical
    FATP_ASSERT_EQ(seq1.size(), seq2.size(), "Sequences should have same length");
    for (size_t i = 0; i < seq1.size(); ++i)
    {
        FATP_ASSERT_EQ(seq1[i], seq2[i], "Seeded sequences should be identical");
    }

    // Reset with different seed should produce different sequence
    policy1.reset_with_seed(99999);
    auto different = policy1.next_id(0, true);
    FATP_ASSERT_TRUE(different.has_value(), "Generation should succeed");
    FATP_ASSERT_TRUE(*different != seq1[0], "Different seed should produce different sequence");

    return true;
}

FATP_TEST_CASE(random_idgenerator_seeded)
{
    // Test that seeded IdGenerator produces reproducible sequences
    // This validates the SFINAE-enabled seeded constructor
    constexpr uint64_t kTestSeed = 42;

    // Create two generators with same seed using the new seeded constructor
    RandomIdGenerator<uint64_t> gen1(seed_tag, kTestSeed);
    RandomIdGenerator<uint64_t> gen2(seed_tag, kTestSeed);

    // Generate sequence from both - should be identical
    std::vector<uint64_t> seq1, seq2;
    for (int i = 0; i < 10; ++i)
    {
        auto id1 = gen1.generate();
        auto id2 = gen2.generate();
        FATP_ASSERT_TRUE(id1.has_value() && id2.has_value(), "Seeded generation should succeed");
        seq1.push_back(*id1);
        seq2.push_back(*id2);
    }

    // Sequences should be identical
    for (size_t i = 0; i < seq1.size(); ++i)
    {
        FATP_ASSERT_EQ(seq1[i], seq2[i], "Seeded IdGenerators should produce identical sequences");
    }

    // Different seed should produce different sequence
    RandomIdGenerator<uint64_t> gen3(seed_tag, 99999);
    auto different = gen3.generate();
    FATP_ASSERT_TRUE(different.has_value(), "Generation should succeed");
    FATP_ASSERT_TRUE(*different != seq1[0], "Different seed should produce different first ID");

    return true;
}

// =============================================================================
// IV. Thread Safety Tests
// =============================================================================

FATP_TEST_CASE(thread_safety)
{
    ThreadSafeIdGenerator<uint64_t> safe_gen(1);

    const size_t num_threads = 4;
    const size_t ids_per_thread = 1000;
    std::vector<std::set<uint64_t>> thread_ids(num_threads);

    auto worker = [&](size_t thread_id) {
        for (size_t i = 0; i < ids_per_thread; ++i)
        {
            auto id = safe_gen.generate();
            if (id)
            {
                thread_ids[thread_id].insert(id.value());
            }
        }
    };

    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Check for collisions across threads
    std::set<uint64_t> all_ids;
    for (const auto& thread_set : thread_ids)
    {
        for (uint64_t id : thread_set)
        {
            FATP_ASSERT_TRUE(all_ids.find(id) == all_ids.end(), "Thread-safety violation: duplicate ID");
            all_ids.insert(id);
        }
    }

    size_t total_expected = num_threads * ids_per_thread;
    FATP_ASSERT_EQ(all_ids.size(), total_expected, "Should generate correct number of unique IDs");

    std::cout << "Generated " << all_ids.size() << " unique IDs across " << num_threads << " threads in "
              << duration.count() << " ms\n";

    return true;
}

FATP_TEST_CASE(concurrent_queries)
{
    // Test concurrent reads (is_active, active_count) alongside writes (generate, release)
    // Validates shared locking correctness for reader-writer scenarios
    ThreadSafeIdGenerator<uint64_t> gen(1);

    constexpr size_t kNumThreads = 4;
    constexpr size_t kOpsPerThread = 500; // Reduced for balanced read/write load

    std::atomic<size_t> query_count{0};
    std::atomic<size_t> generate_count{0};
    std::atomic<bool> stop_flag{false};

    std::vector<std::thread> threads;

    // Writer threads: generate and release IDs
    for (size_t t = 0; t < kNumThreads / 2; ++t)
    {
        threads.emplace_back([&gen, &generate_count, &stop_flag]() {
            std::vector<uint64_t> my_ids;
            my_ids.reserve(kOpsPerThread);

            for (size_t i = 0; i < kOpsPerThread && !stop_flag; ++i)
            {
                auto id = gen.generate();
                if (id)
                {
                    my_ids.push_back(*id);
                    generate_count.fetch_add(1, std::memory_order_relaxed);
                }

                // Release some to exercise concurrent state changes
                if (my_ids.size() > 10 && (i % 3 == 0))
                {
                    (void)gen.release(my_ids.back());
                    my_ids.pop_back();
                }
            }

            // Cleanup remaining IDs
            for (auto id : my_ids)
            {
                (void)gen.release(id);
            }
        });
    }

    // Reader threads: query is_active and active_count
    for (size_t t = 0; t < kNumThreads / 2; ++t)
    {
        threads.emplace_back([&gen, &query_count, &stop_flag]() {
            for (size_t i = 0; i < kOpsPerThread * 2 && !stop_flag; ++i)
            {
                // Query active count
                size_t count = gen.active_count();
                DoNotOptimize(count);

                // Query is_active for various IDs
                for (uint64_t check_id = 1; check_id <= 20; ++check_id)
                {
                    bool active = gen.is_active(check_id);
                    DoNotOptimize(active);
                }

                query_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // Verify consistency after concurrent operations
    FATP_ASSERT_TRUE(query_count > 0, "Query threads should have executed");
    FATP_ASSERT_TRUE(generate_count > 0, "Generate threads should have executed");

    // Final state should be consistent (all cleanup completed)
    FATP_ASSERT_EQ(gen.active_count(), size_t(0), "All IDs should be released after cleanup");

    return true;
}

// =============================================================================
// V. Recycling Policy Tests
// =============================================================================

FATP_TEST_CASE(no_recycling)
{
    using NoRecycleGen = IdGenerator<uint64_t, SequentialAllocationPolicy<uint64_t>, NoRecyclingPolicy<uint64_t>>;

    NoRecycleGen gen(1);

    auto id1 = gen.generate();
    auto id2 = gen.generate();

    (void)gen.release(id1.value());
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(0), "No recycling policy should not store IDs");

    auto id3 = gen.generate();
    FATP_ASSERT_EQ(id3.value(), uint64_t(3), "Should continue sequence without recycling");

    return true;
}

FATP_TEST_CASE(recycling_order)
{
    SimpleIdGenerator<uint64_t> gen(1);

    // Generate several IDs
    auto id1 = gen.generate(); // 1
    auto id2 = gen.generate(); // 2
    auto id3 = gen.generate(); // 3

    // Release in different order
    (void)gen.release(id2.value()); // Release 2 first
    (void)gen.release(id1.value()); // Release 1 second

    // Should get back in FIFO order (first released = first reused)
    auto recycled1 = gen.generate();
    FATP_ASSERT_EQ(recycled1.value(), uint64_t(2), "First recycled should be 2 (FIFO)");

    auto recycled2 = gen.generate();
    FATP_ASSERT_EQ(recycled2.value(), uint64_t(1), "Second recycled should be 1 (FIFO)");

    // Next should be fresh
    auto fresh = gen.generate();
    FATP_ASSERT_EQ(fresh.value(), uint64_t(4), "Should get fresh ID after recycled exhausted");

    return true;
}

FATP_TEST_CASE(min_recycling_policy)
{
    // Test MinRecyclingPolicy - should recycle smallest ID first
    using MinGen = IdGenerator<uint64_t, SequentialAllocationPolicy<uint64_t>, MinRecyclingPolicy<uint64_t>>;
    MinGen gen(1);

    auto id1 = gen.generate(); // 1
    auto id2 = gen.generate(); // 2
    auto id3 = gen.generate(); // 3

    // Release out of order: 3, 1, 2
    (void)gen.release(id3.value());
    (void)gen.release(id1.value());
    (void)gen.release(id2.value());

    // Should get back in sorted order: 1, 2, 3 (smallest first)
    auto r1 = gen.generate();
    FATP_ASSERT_EQ(r1.value(), uint64_t(1), "Should recycle smallest (1) first");

    auto r2 = gen.generate();
    FATP_ASSERT_EQ(r2.value(), uint64_t(2), "Should recycle next smallest (2)");

    auto r3 = gen.generate();
    FATP_ASSERT_EQ(r3.value(), uint64_t(3), "Should recycle next smallest (3)");

    return true;
}

FATP_TEST_CASE(dense_id_generator_alias)
{
    // Verify DenseIdGenerator alias compiles and uses MinRecyclingPolicy
    DenseIdGenerator<uint64_t> gen(0);

    auto id0 = gen.generate(); // 0
    auto id1 = gen.generate(); // 1
    auto id2 = gen.generate(); // 2

    // Release 2 and 0
    (void)gen.release(id2.value());
    (void)gen.release(id0.value());

    // Should get 0 first (smallest)
    auto recycled = gen.generate();
    FATP_ASSERT_EQ(recycled.value(), uint64_t(0), "DenseIdGenerator should recycle smallest first");

    return true;
}

FATP_TEST_CASE(retry_logic_collision)
{
    // Test that retry loop handles collisions in RandomAllocationPolicy
    // Use small type to increase collision probability
    RandomIdGenerator<uint8_t> gen;

    // Generate many IDs - retry loop should handle collisions
    std::set<uint8_t> generated;
    size_t success_count = 0;
    size_t max_attempts = 100;

    for (size_t i = 0; i < max_attempts && success_count < 50; ++i)
    {
        auto id = gen.generate();
        if (id.has_value())
        {
            // Verify no duplicates
            FATP_ASSERT_TRUE(generated.find(id.value()) == generated.end(),
                             "Retry logic should prevent duplicate returns");
            generated.insert(id.value());
            ++success_count;
        }
    }

    // Should have generated a reasonable number of unique IDs
    FATP_ASSERT_TRUE(success_count >= 20, "Retry loop should allow multiple successful generations");

    return true;
}

FATP_TEST_CASE(basic_batch_generation)
{
    SimpleIdGenerator<uint64_t> gen(1);

    // Generate batch of 10 IDs
    auto batch = gen.generate_batch(10);
    FATP_ASSERT_TRUE(batch.has_value(), "Batch generation should succeed");
    FATP_ASSERT_EQ(batch.value().size(), size_t(10), "Should generate 10 IDs");

    // Verify all IDs are unique and sequential
    std::set<uint64_t> unique_ids(batch.value().begin(), batch.value().end());
    FATP_ASSERT_EQ(unique_ids.size(), size_t(10), "All batch IDs should be unique");

    // Verify sequential: should be 1-10
    FATP_ASSERT_EQ(batch.value()[0], uint64_t(1), "First ID should be 1");
    FATP_ASSERT_EQ(batch.value()[9], uint64_t(10), "Last ID should be 10");

    // Verify active count
    FATP_ASSERT_EQ(gen.active_count(), size_t(10), "All 10 IDs should be active");

    // Empty batch should succeed
    auto empty_batch = gen.generate_batch(0);
    FATP_ASSERT_TRUE(empty_batch.has_value(), "Empty batch should succeed");
    FATP_ASSERT_EQ(empty_batch.value().size(), size_t(0), "Empty batch should have 0 IDs");

    return true;
}

FATP_TEST_CASE(batch_with_recycling)
{
    SimpleIdGenerator<uint64_t> gen(1);

    // Generate and release some IDs
    auto id1 = gen.generate(); // 1
    auto id2 = gen.generate(); // 2
    auto id3 = gen.generate(); // 3

    (void)gen.release(id2.value()); // 2 recycled
    (void)gen.release(id1.value()); // 1 recycled

    // Batch should use recycled IDs first (FIFO: 2, then 1)
    auto batch = gen.generate_batch(3);
    FATP_ASSERT_TRUE(batch.has_value(), "Batch with recycling should succeed");
    FATP_ASSERT_EQ(batch.value().size(), size_t(3), "Should generate 3 IDs");

    // First two should be recycled (2, 1), third should be fresh (4)
    FATP_ASSERT_EQ(batch.value()[0], uint64_t(2), "First should be recycled 2");
    FATP_ASSERT_EQ(batch.value()[1], uint64_t(1), "Second should be recycled 1");
    FATP_ASSERT_EQ(batch.value()[2], uint64_t(4), "Third should be fresh 4");

    return true;
}

FATP_TEST_CASE(threadsafe_batch_generation)
{
    ThreadSafeIdGenerator<uint64_t> gen(1);

    constexpr size_t batch_size = 100;
    constexpr size_t num_threads = 4;

    std::vector<std::vector<uint64_t>> thread_ids(num_threads);
    std::vector<std::thread> threads;

    for (size_t t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([&gen, &thread_ids, t]() {
            auto batch = gen.generate_batch(batch_size);
            if (batch.has_value())
            {
                thread_ids[t] = std::move(batch.value());
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // Verify no collisions across threads
    std::set<uint64_t> all_ids;
    for (const auto& ids : thread_ids)
    {
        FATP_ASSERT_EQ(ids.size(), batch_size, "Each thread should get full batch");
        for (uint64_t id : ids)
        {
            FATP_ASSERT_TRUE(all_ids.find(id) == all_ids.end(),
                             "Batch generation should produce unique IDs across threads");
            all_ids.insert(id);
        }
    }

    FATP_ASSERT_EQ(all_ids.size(), num_threads * batch_size, "Total unique IDs should equal threads * batch_size");

    return true;
}

FATP_TEST_CASE(batch_overflow_rollback)
{
    // Use small type to test overflow and rollback
    SimpleIdGenerator<uint8_t> gen(250);

    // Generate 5 IDs: 250, 251, 252, 253, 254
    auto batch1 = gen.generate_batch(5);
    FATP_ASSERT_TRUE(batch1.has_value(), "First batch should succeed");
    FATP_ASSERT_EQ(batch1.value().size(), size_t(5), "Should have 5 IDs");

    // Only 255 left. Requesting 3 should fail (overflow after 1)
    // and rollback any partial success
    auto batch2 = gen.generate_batch(3);
    FATP_ASSERT_TRUE(!batch2.has_value(), "Batch exceeding space should fail");
    FATP_ASSERT_TRUE(batch2.error() == IdError::Overflow, "Error should be Overflow");

    // Enhanced verification: Check individual ID states after rollback
    for (auto id : {uint8_t{250}, uint8_t{251}, uint8_t{252}, uint8_t{253}, uint8_t{254}})
    {
        FATP_ASSERT_TRUE(gen.is_active(id), "Original IDs should remain active after rollback");
    }

    // Active count should still be 5 (rollback worked)
    FATP_ASSERT_EQ(gen.active_count(), size_t(5), "Rollback should restore state");

    // No IDs should be in recycle pool (rollback discards high IDs)
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(0), "No partial IDs should be recycled after rollback");

    // Can still generate the remaining ID
    auto last = gen.generate();
    FATP_ASSERT_TRUE(last.has_value(), "Should still generate remaining ID");
    FATP_ASSERT_EQ(last.value(), uint8_t(255), "Last ID should be 255");

    return true;
}

FATP_TEST_CASE(batch_rollback_preserves_density)
{
    // Test that rollback preserves density for MinRecyclingPolicy
    // Only recycled IDs (below pre-batch max) should be returned to pool
    // Newly generated IDs (above pre-batch max) are discarded, not recycled
    DenseIdGenerator<uint8_t> gen(250);

    // Generate 5 IDs: 250, 251, 252, 253, 254
    auto batch1 = gen.generate_batch(5);
    FATP_ASSERT_TRUE(batch1.has_value(), "First batch should succeed");
    FATP_ASSERT_EQ(batch1->size(), size_t(5), "Should have 5 IDs");

    // Release 251 and 253 to create recycled IDs
    (void)gen.release(uint8_t(251));
    (void)gen.release(uint8_t(253));
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(2), "Should have 2 recycled IDs");

    // Current state: Active {250, 252, 254}, Recycled {251, 253} (min-sorted)
    // Pre-batch max is 254

    // Attempt batch that will fail:
    // First 2 will come from recycle pool (251, 253 - min-first order)
    // Then try fresh ID 255, which succeeds
    // Then overflow on next ID
    auto batch2 = gen.generate_batch(4);
    FATP_ASSERT_TRUE(!batch2.has_value(), "Batch exceeding space should fail");
    FATP_ASSERT_TRUE(batch2.error() == IdError::Overflow, "Error should be Overflow");

    // Verify rollback preserved density:
    // - IDs 251, 253 were recycled (below pre-batch max 254), should be back in pool
    // - ID 255 was new (above pre-batch max 254), should be DISCARDED, not recycled
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(2), "Only pre-existing recycled IDs (251, 253) should return");

    // Verify the recycled IDs are the original ones (251, 253)
    // Generate to consume from recycle pool
    auto id1 = gen.generate();
    FATP_ASSERT_TRUE(id1.has_value(), "Should get recycled ID");
    FATP_ASSERT_EQ(id1.value(), uint8_t(251), "MinRecyclingPolicy should return smallest first");

    auto id2 = gen.generate();
    FATP_ASSERT_TRUE(id2.has_value(), "Should get recycled ID");
    FATP_ASSERT_EQ(id2.value(), uint8_t(253), "Should return next smallest");

    // Now recycle pool is empty, next should be fresh ID 255
    auto id3 = gen.generate();
    FATP_ASSERT_TRUE(id3.has_value(), "Should get fresh ID");
    FATP_ASSERT_EQ(id3.value(), uint8_t(255), "Fresh ID should be 255");

    return true;
}

FATP_TEST_CASE(release_batch_basic)
{
    SimpleIdGenerator<uint64_t> gen(1);

    // Generate a batch
    auto batch = gen.generate_batch(10);
    FATP_ASSERT_TRUE(batch.has_value(), "Batch generation should succeed");
    FATP_ASSERT_EQ(gen.active_count(), size_t(10), "Should have 10 active IDs");

    // Release them all in one call
    auto result = gen.release_batch(batch.value());
    FATP_ASSERT_TRUE(result.has_value(), "Batch release should succeed");
    FATP_ASSERT_EQ(gen.active_count(), size_t(0), "All IDs should be released");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(10), "All IDs should be recycled");

    return true;
}

FATP_TEST_CASE(release_batch_error_handling)
{
    SimpleIdGenerator<uint64_t> gen(1);

    // Generate some IDs
    auto id1 = gen.generate();
    auto id2 = gen.generate();
    auto id3 = gen.generate();

    // Try to release with an invalid ID in the middle
    std::vector<uint64_t> to_release = {id1.value(), 999, id3.value()};
    auto result = gen.release_batch(to_release);

    // Should fail on the invalid ID
    FATP_ASSERT_TRUE(!result.has_value(), "Should fail on invalid ID");
    FATP_ASSERT_TRUE(result.error() == IdError::InvalidRelease, "Error should be InvalidRelease");

    // First ID should be released, but second and third not processed
    FATP_ASSERT_TRUE(!gen.is_active(id1.value()), "First ID should be released before error");
    FATP_ASSERT_TRUE(gen.is_active(id2.value()), "Second ID should still be active");
    FATP_ASSERT_TRUE(gen.is_active(id3.value()), "Third ID should still be active (not processed)");

    return true;
}

// =============================================================================
// VI. Edge Cases
// =============================================================================

FATP_TEST_CASE(edge_cases)
{
    // Test with base_id at 0
    SimpleIdGenerator<uint64_t> gen_zero(0);
    auto id_zero = gen_zero.generate();
    FATP_ASSERT_EQ(id_zero.value(), uint64_t(0), "Should handle base_id of 0");

    // Test reset functionality
    SimpleIdGenerator<uint64_t> gen(100);
    (void)gen.generate();
    (void)gen.generate();
    FATP_ASSERT_EQ(gen.active_count(), size_t(2), "Should have 2 active IDs");

    gen.reset();
    FATP_ASSERT_EQ(gen.active_count(), size_t(0), "Reset should clear active IDs");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(0), "Reset should clear recycled IDs");

    auto id_after_reset = gen.generate();
    FATP_ASSERT_EQ(id_after_reset.value(), uint64_t(100), "Should restart from base after reset");

    // Test is_active query
    FATP_ASSERT_TRUE(gen.is_active(id_after_reset.value()), "Should report ID as active");
    (void)gen.release(id_after_reset.value());
    FATP_ASSERT_TRUE(!gen.is_active(id_after_reset.value()), "Should report ID as inactive");

    return true;
}

FATP_TEST_CASE(double_release)
{
    SimpleIdGenerator<uint64_t> gen(1);

    auto id = gen.generate();
    FATP_ASSERT_TRUE(id.has_value(), "Generation should succeed");

    auto release1 = gen.release(id.value());
    FATP_ASSERT_TRUE(release1.has_value(), "First release should succeed");

    auto release2 = gen.release(id.value());
    FATP_ASSERT_TRUE(!release2.has_value(), "Second release should fail");
    FATP_ASSERT_TRUE(release2.error() == IdError::InvalidRelease, "Should get InvalidRelease");

    return true;
}

FATP_TEST_CASE(overflow_boundary)
{
    // Test near overflow boundary with small type
    SimpleIdGenerator<uint8_t> gen(253);

    auto id1 = gen.generate(); // 253
    auto id2 = gen.generate(); // 254
    auto id3 = gen.generate(); // 255

    FATP_ASSERT_EQ(id1.value(), uint8_t(253), "Should get 253");
    FATP_ASSERT_EQ(id2.value(), uint8_t(254), "Should get 254");
    FATP_ASSERT_EQ(id3.value(), uint8_t(255), "Should get 255 (max)");

    // Next should overflow
    auto id4 = gen.generate();
    FATP_ASSERT_TRUE(!id4.has_value(), "Should fail on overflow");
    FATP_ASSERT_TRUE(id4.error() == IdError::Overflow, "Should get Overflow error");

    // But recycling should still work
    (void)gen.release(id2.value());
    auto recycled = gen.generate();
    FATP_ASSERT_EQ(recycled.value(), uint8_t(254), "Should be able to reuse recycled ID");

    return true;
}

FATP_TEST_CASE(overflow_exhaustion_tracking)
{
    // Test that once we reach max, subsequent calls correctly return Overflow
    // This validates the mExhausted flag in SequentialAllocationPolicy
    SimpleIdGenerator<uint8_t> gen(254);

    auto id254 = gen.generate(); // 254
    auto id255 = gen.generate(); // 255 (max)

    FATP_ASSERT_EQ(id254.value(), uint8_t(254), "Should get 254");
    FATP_ASSERT_EQ(id255.value(), uint8_t(255), "Should get 255 (max)");

    // Multiple overflow attempts should all return Overflow (not AlreadyInUse)
    for (int i = 0; i < 5; ++i)
    {
        auto overflow = gen.generate();
        FATP_ASSERT_TRUE(!overflow.has_value(), "Should fail on overflow");
        FATP_ASSERT_TRUE(overflow.error() == IdError::Overflow, "Should get Overflow error, not AlreadyInUse");
    }

    // After release and recycle, should still work
    (void)gen.release(id254.value());
    auto recycled = gen.generate();
    FATP_ASSERT_EQ(recycled.value(), uint8_t(254), "Should recycle 254");

    // Fresh generation should still fail
    auto overflow2 = gen.generate();
    FATP_ASSERT_TRUE(!overflow2.has_value(), "Should still fail on overflow");
    FATP_ASSERT_TRUE(overflow2.error() == IdError::Overflow, "Should be Overflow");

    return true;
}

FATP_TEST_CASE(batch_rollback_reverts_counter)
{
    // Test that batch rollback properly reverts the allocation policy counter
    // to prevent sequence gaps in sequential generation
    SimpleIdGenerator<uint8_t> gen(250);

    // Generate some IDs
    auto id250 = gen.generate();
    auto id251 = gen.generate();
    FATP_ASSERT_EQ(id250.value(), uint8_t(250), "Should get 250");
    FATP_ASSERT_EQ(id251.value(), uint8_t(251), "Should get 251");

    // Release 251 to recycle pool
    (void)gen.release(id251.value());

    // Now try batch generation that will eventually fail
    // With range [250, 255], we have 250 active, 251 recycled, 252-255 free
    // Request 6 IDs: should fail partway through
    auto batch = gen.generate_batch(10);

    // The batch should fail (only 5 IDs available: 251 recycled + 252,253,254,255)
    FATP_ASSERT_TRUE(!batch.has_value(), "Batch should fail");
    FATP_ASSERT_TRUE(batch.error() == IdError::Overflow, "Should be overflow");

    // After rollback, we should be able to generate the IDs we failed to batch
    // The counter should have been reverted
    auto id251_retry = gen.generate();
    FATP_ASSERT_TRUE(id251_retry.has_value(), "Should get recycled 251");
    FATP_ASSERT_EQ(id251_retry.value(), uint8_t(251), "Should be 251");

    auto id252 = gen.generate();
    FATP_ASSERT_TRUE(id252.has_value(), "Should get 252");
    FATP_ASSERT_EQ(id252.value(), uint8_t(252), "Should be 252");

    return true;
}

// =============================================================================
// VII. Custom Policy Tests
// =============================================================================

FATP_TEST_CASE(custom_allocation_policy)
{
    // Custom policy that only generates even IDs
    struct EvenOnlyPolicy
    {
        uint64_t mNext = 0;

        explicit EvenOnlyPolicy(uint64_t base = 0)
            : mNext((base + 1) & ~uint64_t(1))
        {
        }

        std::optional<uint64_t> next_id(uint64_t, bool) noexcept
        {
            uint64_t result = mNext;
            mNext += 2;
            return result;
        }

        void reset(uint64_t base = 0) noexcept
        {
            mNext = (base + 1) & ~uint64_t(1);
        }
    };

    IdGenerator<uint64_t, EvenOnlyPolicy> gen(0);

    auto id1 = gen.generate();
    auto id2 = gen.generate();
    auto id3 = gen.generate();

    FATP_ASSERT_TRUE(id1.has_value(), "Custom policy generation should succeed");
    FATP_ASSERT_TRUE((id1.value() % 2) == 0, "ID should be even");
    FATP_ASSERT_TRUE((id2.value() % 2) == 0, "ID should be even");
    FATP_ASSERT_TRUE((id3.value() % 2) == 0, "ID should be even");

    // Verify sequential even numbers
    FATP_ASSERT_EQ(id2.value(), id1.value() + 2, "IDs should be consecutive even numbers");
    FATP_ASSERT_EQ(id3.value(), id2.value() + 2, "IDs should be consecutive even numbers");

    return true;
}

FATP_TEST_CASE(bounded_allocation)
{
    // Test BoundedSequentialAllocationPolicy for custom ID ranges
    // Use the policy directly since IdGenerator's constructor only passes base_id
    BoundedSequentialAllocationPolicy<uint8_t> policy(250, 253);

    std::vector<uint8_t> ids;

    // Should successfully generate 4 IDs: 250, 251, 252, 253
    for (int i = 0; i < 5; ++i)
    {
        auto id = policy.next_id(ids.empty() ? 250 : ids.back(), ids.empty());
        if (!id.has_value())
        {
            break;
        }
        ids.push_back(*id);
    }

    FATP_ASSERT_EQ(ids.size(), size_t(4), "Should generate 4 IDs (250-253)");
    FATP_ASSERT_EQ(ids[0], uint8_t(250), "First ID should be 250");
    FATP_ASSERT_EQ(ids[1], uint8_t(251), "Second ID should be 251");
    FATP_ASSERT_EQ(ids[2], uint8_t(252), "Third ID should be 252");
    FATP_ASSERT_EQ(ids[3], uint8_t(253), "Fourth ID should be 253 (max bound)");

    // Verify overflow after bound
    auto overflow = policy.next_id(253, false);
    FATP_ASSERT_TRUE(!overflow.has_value(), "Should overflow after max bound");

    return true;
}

FATP_TEST_CASE(dirty_max_smaller_id)
{
    // Test edge case: inserting smaller ID when max is invalid.
    // Uses custom policy to generate IDs: 20, 10, 5 (decreasing order).
    // This is unrealistic but tests the tracker's robustness.

    struct DecreasingPolicy
    {
        std::vector<uint64_t> mSequence;
        size_t mIndex = 0;

        explicit DecreasingPolicy(uint64_t)
            : mSequence{20, 10, 5, 25}
        {
        }

        std::optional<uint64_t> next_id(uint64_t, bool) noexcept
        {
            if (mIndex >= mSequence.size())
            {
                return std::nullopt;
            }
            return mSequence[mIndex++];
        }
    };

    IdGenerator<uint64_t, DecreasingPolicy, NoRecyclingPolicy<uint64_t>> gen(0);

    // Generate 20 - Active: {20}, max=20
    auto id20 = gen.generate();
    FATP_ASSERT_TRUE(id20.has_value(), "Generate should succeed");
    FATP_ASSERT_EQ(id20.value(), uint64_t(20), "First ID is 20");

    // Generate 10 - Active: {10, 20}, max=20 (10 < 20, no update)
    auto id10 = gen.generate();
    FATP_ASSERT_TRUE(id10.has_value(), "Generate should succeed");
    FATP_ASSERT_EQ(id10.value(), uint64_t(10), "Second ID is 10");

    // Release 20 (the max) - Active: {10}, max_valid_=false
    (void)gen.release(id20.value());
    FATP_ASSERT_EQ(gen.active_count(), size_t(1), "Should have 1 active ID");

    // Generate 5 - With OLD bug: max would become 5 (WRONG!)
    // With fix: max_valid_ stays false, insert(5) doesn't touch mMax
    auto id5 = gen.generate();
    FATP_ASSERT_TRUE(id5.has_value(), "Generate should succeed");
    FATP_ASSERT_EQ(id5.value(), uint64_t(5), "Third ID is 5");

    // Active: {5, 10}, max should be 10 (not 5!)
    FATP_ASSERT_EQ(gen.active_count(), size_t(2), "Should have 2 active IDs");

    // Generate 25 - max_element() must recompute, find max=10, return it
    // Then policy gives us 25, we insert it, and since 25 > 10 and max was just
    // validated, max becomes 25
    auto id25 = gen.generate();
    FATP_ASSERT_TRUE(id25.has_value(), "Generate should succeed");
    FATP_ASSERT_EQ(id25.value(), uint64_t(25), "Fourth ID is 25");

    // Final state: {5, 10, 25}
    FATP_ASSERT_EQ(gen.active_count(), size_t(3), "Should have 3 active IDs");
    FATP_ASSERT_TRUE(gen.is_active(5), "5 should be active");
    FATP_ASSERT_TRUE(gen.is_active(10), "10 should be active");
    FATP_ASSERT_TRUE(gen.is_active(25), "25 should be active");

    return true;
}

// =============================================================================
// VIII. Active ID Tracking Tests
// =============================================================================

FATP_TEST_CASE(active_id_tracking)
{
    // Test the ActiveIdTracker (unordered_set with lazy max)
    SimpleIdGenerator<uint64_t> gen(1);

    auto id1 = gen.generate();
    auto id2 = gen.generate();
    auto id3 = gen.generate();

    FATP_ASSERT_TRUE(id1.has_value() && id2.has_value() && id3.has_value(), "Generation should succeed");
    FATP_ASSERT_EQ(id1.value(), uint64_t(1), "First ID should be 1");
    FATP_ASSERT_EQ(id2.value(), uint64_t(2), "Second ID should be 2");
    FATP_ASSERT_EQ(id3.value(), uint64_t(3), "Third ID should be 3");

    // Release max (id3) - should trigger lazy max recomputation
    (void)gen.release(id3.value());

    // Generate new ID - with ImmediateRecyclingPolicy, recycled IDs are
    // returned first, so we should get 3 (the recycled ID), not 4
    auto id4 = gen.generate();
    FATP_ASSERT_TRUE(id4.has_value(), "Generation after max release should succeed");
    FATP_ASSERT_EQ(id4.value(), uint64_t(3), "Should get recycled ID (3) with ImmediateRecyclingPolicy");

    return true;
}

FATP_TEST_CASE(lazy_max_recompute)
{
    // Test lazy max recomputation in ActiveIdTracker
    SimpleIdGenerator<uint64_t> gen(1);

    // Generate 5 IDs: 1, 2, 3, 4, 5
    std::vector<uint64_t> ids;
    for (int i = 0; i < 5; ++i)
    {
        auto id = gen.generate();
        FATP_ASSERT_TRUE(id.has_value(), "Generation should succeed");
        ids.push_back(id.value());
    }

    // Release max (5) - invalidates cached max
    (void)gen.release(ids[4]);
    // Release 4 - still not the cached max (which is now invalid)
    (void)gen.release(ids[3]);
    // Release 3
    (void)gen.release(ids[2]);

    // Now generate - should recompute max from remaining {1, 2}
    // Recycling should give us back one of 3, 4, 5 first
    auto new_id = gen.generate();
    FATP_ASSERT_TRUE(new_id.has_value(), "Generation should succeed");

    // The recycled ID should be 5 (FIFO - first released)
    FATP_ASSERT_EQ(new_id.value(), uint64_t(5), "Should get first recycled ID (FIFO)");

    return true;
}

FATP_TEST_CASE(dirty_max_insert)
{
    // Test that inserting when max is invalid doesn't corrupt max tracking.
    // The key scenario: after releasing max, the next max_element() call should
    // correctly recompute from remaining IDs.

    // Note: SequentialAllocationPolicy maintains internal counter that never decreases,
    // so released IDs are NOT reused (that's what recycling is for).

    using Gen = IdGenerator<uint64_t,
                            SequentialAllocationPolicy<uint64_t>,
                            NoRecyclingPolicy<uint64_t>,
                            fat_p::id_generator::ExpectedErrorPolicy<uint64_t, IdError>,
                            SingleThreadedPolicy>;

    Gen gen(10); // Start at 10

    // Generate: 10, 11, 12
    auto id10 = gen.generate();
    auto id11 = gen.generate();
    auto id12 = gen.generate();
    FATP_ASSERT_TRUE(id10.has_value() && id11.has_value() && id12.has_value(), "Generate should succeed");
    FATP_ASSERT_EQ(id10.value(), uint64_t(10), "First ID");
    FATP_ASSERT_EQ(id11.value(), uint64_t(11), "Second ID");
    FATP_ASSERT_EQ(id12.value(), uint64_t(12), "Third ID (max)");

    // Release max (12) - invalidates cached max. Active: {10, 11}
    auto rel = gen.release(id12.value());
    FATP_ASSERT_TRUE(rel.has_value(), "Release should succeed");

    // Generate next - policy's internal counter is at 13, max_element()=11
    // Policy returns max(13, 11+1) = 13
    auto id13 = gen.generate();
    FATP_ASSERT_TRUE(id13.has_value(), "Generate should succeed");
    FATP_ASSERT_EQ(id13.value(), uint64_t(13), "Sequential policy advances past released IDs");

    // Now test max tracking after dirty insert:
    // Active: {10, 11, 13}, max should be 13
    // Release 13 -> max_valid_ = false, Active: {10, 11}
    (void)gen.release(id13.value());

    // Release 11 -> Active: {10}, max_valid_ still false (11 != max which was 13)
    (void)gen.release(id11.value());

    // Generate - max_element() should scan {10}, find max=10
    // Policy returns max(14, 10+1) = 14
    auto id14 = gen.generate();
    FATP_ASSERT_TRUE(id14.has_value(), "Generate should succeed");
    FATP_ASSERT_EQ(id14.value(), uint64_t(14), "Policy continues advancing");

    // Verify correct tracking - should have {10, 14}
    FATP_ASSERT_EQ(gen.active_count(), size_t(2), "Should have 2 active IDs");
    FATP_ASSERT_TRUE(gen.is_active(10), "ID 10 should be active");
    FATP_ASSERT_TRUE(gen.is_active(14), "ID 14 should be active");
    FATP_ASSERT_TRUE(!gen.is_active(11), "ID 11 should not be active");
    FATP_ASSERT_TRUE(!gen.is_active(12), "ID 12 should not be active");
    FATP_ASSERT_TRUE(!gen.is_active(13), "ID 13 should not be active");

    return true;
}

// =============================================================================
// Benchmarks
// =============================================================================

// Benchmark configuration constants
constexpr size_t kBenchmarkIterations = 100000;
constexpr size_t kStandardWarmup = 1000;
constexpr size_t kReducedIterations = 10000; // For slower operations
} // namespace fat_p::testing::idgenerator

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_IdGenerator()
{
    FATP_PRINT_HEADER(ID GENERATOR)

    TestRunner runner;

    // Basic functionality
    FATP_RUN_TEST_NS(runner, idgenerator, basic_sequential);
    FATP_RUN_TEST_NS(runner, idgenerator, strong_id_integration);
    FATP_RUN_TEST_NS(runner, idgenerator, error_handling);

    // RAII guards
    FATP_RUN_TEST_NS(runner, idgenerator, raii_guard);
    FATP_RUN_TEST_NS(runner, idgenerator, guard_move_semantics);
    FATP_RUN_TEST_NS(runner, idgenerator, guard_default_ctor);

    // Random allocation
    FATP_RUN_TEST_NS(runner, idgenerator, random_allocation);
    FATP_RUN_TEST_NS(runner, idgenerator, random_small_type);
    FATP_RUN_TEST_NS(runner, idgenerator, random_seed_reproducibility);
    FATP_RUN_TEST_NS(runner, idgenerator, random_idgenerator_seeded);

    // Thread safety
    FATP_RUN_TEST_NS(runner, idgenerator, thread_safety);
    FATP_RUN_TEST_NS(runner, idgenerator, concurrent_queries);

    // Recycling
    FATP_RUN_TEST_NS(runner, idgenerator, no_recycling);
    FATP_RUN_TEST_NS(runner, idgenerator, recycling_order);
    FATP_RUN_TEST_NS(runner, idgenerator, min_recycling_policy);
    FATP_RUN_TEST_NS(runner, idgenerator, dense_id_generator_alias);
    FATP_RUN_TEST_NS(runner, idgenerator, retry_logic_collision);

    // Batch generation
    FATP_RUN_TEST_NS(runner, idgenerator, basic_batch_generation);
    FATP_RUN_TEST_NS(runner, idgenerator, batch_with_recycling);
    FATP_RUN_TEST_NS(runner, idgenerator, threadsafe_batch_generation);
    FATP_RUN_TEST_NS(runner, idgenerator, batch_overflow_rollback);
    FATP_RUN_TEST_NS(runner, idgenerator, batch_rollback_preserves_density);
    FATP_RUN_TEST_NS(runner, idgenerator, release_batch_basic);
    FATP_RUN_TEST_NS(runner, idgenerator, release_batch_error_handling);

    // Edge cases
    FATP_RUN_TEST_NS(runner, idgenerator, edge_cases);
    FATP_RUN_TEST_NS(runner, idgenerator, double_release);
    FATP_RUN_TEST_NS(runner, idgenerator, overflow_boundary);
    FATP_RUN_TEST_NS(runner, idgenerator, overflow_exhaustion_tracking);
    FATP_RUN_TEST_NS(runner, idgenerator, batch_rollback_reverts_counter);

    // Custom policies
    FATP_RUN_TEST_NS(runner, idgenerator, custom_allocation_policy);
    FATP_RUN_TEST_NS(runner, idgenerator, bounded_allocation);
    FATP_RUN_TEST_NS(runner, idgenerator, dirty_max_smaller_id);

    // Active ID tracking (unordered_set with lazy max)
    FATP_RUN_TEST_NS(runner, idgenerator, active_id_tracking);
    FATP_RUN_TEST_NS(runner, idgenerator, lazy_max_recompute);
    FATP_RUN_TEST_NS(runner, idgenerator, dirty_max_insert);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_IdGenerator() ? 0 : 1;
}
#endif
