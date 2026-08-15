/**
 * @file test_IdGenerator.cpp
 * @brief Comprehensive unit tests for IdGenerator.h
 *
 * Test Configuration:
 * - C++ Standard: C++20 (the include chain hard-errors below it)
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
  namespace: fat_p::testing::idgenerator
  layer: Testing
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

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <set>
#include <thread>
#include <vector>

#include "ConcurrencyPolicies.h"
#include "Expected.h"
#include "FatPTest.h"
#include "IdGenerator.h"
#include "StrongId.h"

// -----------------------------------------------------------------------------
// Global allocation counter, declared before first use.
//
// Replacing global operator new is a heavy instrument and is used deliberately:
// the return-credit mechanism's whole claim is about WHERE allocation happens,
// and no count-based oracle can see that. See allocprobe below for the arming
// interface. When disarmed -- which is every test but two -- this costs one
// relaxed atomic load per allocation.
//
// Aligned (over-aligned) allocations are deliberately not replaced: nothing on
// the measured paths over-aligns, and replacing them widens the blast radius
// across the whole executable for no coverage.
// -----------------------------------------------------------------------------
namespace fat_p::testing::idgenerator::allocprobe
{
extern std::atomic<bool> counting;
extern std::atomic<size_t> allocations;
extern std::atomic<long long> fail_at;
} // namespace fat_p::testing::idgenerator::allocprobe

namespace
{
void* fatp_probe_allocate(std::size_t bytes)
{
    namespace probe = fat_p::testing::idgenerator::allocprobe;

    if (probe::counting.load(std::memory_order_relaxed))
    {
        const long long index =
            static_cast<long long>(probe::allocations.fetch_add(1, std::memory_order_relaxed));
        const long long fail = probe::fail_at.load(std::memory_order_relaxed);
        if (fail >= 0 && index == fail)
        {
            probe::fail_at.store(-1, std::memory_order_relaxed);
            throw std::bad_alloc();
        }
    }

    void* p = std::malloc(bytes == 0 ? 1 : bytes);
    if (p == nullptr)
    {
        throw std::bad_alloc();
    }
    return p;
}
} // namespace

void* operator new(std::size_t bytes)
{
    return fatp_probe_allocate(bytes);
}
void* operator new[](std::size_t bytes)
{
    return fatp_probe_allocate(bytes);
}
void operator delete(void* p) noexcept
{
    std::free(p);
}
void operator delete[](void* p) noexcept
{
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept
{
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept
{
    std::free(p);
}

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

// =============================================================================
// Coverage added after the component review. Each case here closes a gap where
// a documented behavior had no test, or where an existing test would still have
// passed if the behavior it names were removed.
// =============================================================================

// The suite never produced IdError::AlreadyInUse. Only a colliding allocation
// policy can, and only when the space is full enough that 100 retries all land
// on live IDs.
FATP_TEST_CASE(already_in_use_is_reachable)
{
    RandomIdGenerator<uint8_t> gen;

    size_t generated = 0;
    IdError last = IdError::Overflow;
    bool saw_already_in_use = false;

    for (int i = 0; i < 1000; ++i)
    {
        auto r = gen.generate();
        if (r.has_value())
        {
            ++generated;
            continue;
        }
        last = r.error();
        saw_already_in_use = (last == IdError::AlreadyInUse);
        break;
    }

    FATP_ASSERT_TRUE(saw_already_in_use,
                     "a saturated random generator reports AlreadyInUse, not Overflow");
    FATP_ASSERT_TRUE(generated > 0, "it generated before saturating");
    return true;
}

// The collision retry loop had no test that would notice its removal: with
// kMaxRetries == 1 a random generator fails as soon as one draw collides.
// Filling most of a small space makes collisions near-certain, so a generator
// that still succeeds proves the loop retried.
FATP_TEST_CASE(collision_retry_loop_is_load_bearing)
{
    RandomIdGenerator<uint8_t> gen;

    size_t count = 0;
    while (count < 200)
    {
        auto r = gen.generate();
        if (!r.has_value())
        {
            break;
        }
        ++count;
    }

    FATP_ASSERT_TRUE(count >= 200,
                     "reaching 200 of 256 random IDs requires the retry loop; "
                     "a single-attempt policy would fail far earlier");
    return true;
}

// overflow_exhaustion_tracking never validated the exhausted latch it is named
// for: once exhausted, EVERY later call must fail, including after a release
// that frees an ID below the maximum.
FATP_TEST_CASE(exhausted_latch_holds_until_reset)
{
    using NoRec = IdGenerator<uint8_t,
                              SequentialAllocationPolicy<uint8_t>,
                              NoRecyclingPolicy<uint8_t>>;
    NoRec gen(250);

    while (gen.generate().has_value())
    {
    }

    auto after = gen.generate();
    FATP_ASSERT_FALSE(after.has_value(), "still exhausted");
    FATP_ASSERT_TRUE(after.error() == IdError::Overflow, "and reports Overflow");

    // Releasing does not un-exhaust a non-recycling generator.
    (void)gen.release(250);
    auto after_release = gen.generate();
    FATP_ASSERT_FALSE(after_release.has_value(),
                      "release must not clear the exhausted latch under NoRecyclingPolicy");

    // reset() is the documented way out.
    gen.reset();
    auto after_reset = gen.generate();
    FATP_ASSERT_TRUE(after_reset.has_value(), "reset clears exhaustion");
    FATP_ASSERT_EQ(*after_reset, uint8_t(250), "and restarts at the base");
    return true;
}

// Batch rollback must restore the recycle pool EXACTLY. The previous
// implementation guessed provenance from the pre-batch maximum and silently
// dropped pooled IDs; neither existing rollback test asserted recycled_count().
FATP_TEST_CASE(batch_rollback_restores_the_pool_exactly)
{
    // Case 1: active set empty at batch start. pre_batch_max was nullopt here,
    // which discarded the ENTIRE pool.
    {
        DenseIdGenerator<uint8_t> gen(250);
        auto first = gen.generate_batch(6);
        FATP_ASSERT_TRUE(first.has_value(), "initial batch");

        std::vector<uint8_t> ids(first->begin(), first->end());
        auto released = gen.release_batch(ids);
        FATP_ASSERT_TRUE(released.has_value(), "release_batch");

        const size_t pooled = gen.recycled_count();
        FATP_ASSERT_EQ(pooled, size_t(6), "six IDs pooled");
        FATP_ASSERT_EQ(gen.active_count(), size_t(0), "active set is empty");

        auto second = gen.generate_batch(7);
        FATP_ASSERT_FALSE(second.has_value(), "the second batch cannot be satisfied");
        FATP_ASSERT_EQ(gen.recycled_count(), pooled, "every pooled ID returned to the pool");
        FATP_ASSERT_EQ(gen.active_count(), size_t(0), "and nothing stayed active");
    }

    // Case 2: a pooled ID ABOVE the current active maximum.
    {
        DenseIdGenerator<uint8_t> gen(0);
        for (int i = 0; i < 256; ++i)
        {
            (void)gen.generate();
        }
        (void)gen.release(255);
        const size_t pooled = gen.recycled_count();
        FATP_ASSERT_EQ(pooled, size_t(1), "one ID pooled, above the active max of 254");

        auto batch = gen.generate_batch(2);
        FATP_ASSERT_FALSE(batch.has_value(), "batch fails after consuming the pooled ID");
        FATP_ASSERT_EQ(gen.recycled_count(), pooled, "the pooled ID came back");
        FATP_ASSERT_FALSE(gen.is_active(255), "and is not left active");
    }
    return true;
}

// revert() must rewind the allocation counter by exactly what it advanced.
// Issuing the top-of-range ID parks the counter instead of advancing it, so a
// naive revert re-issued an ID that had already been handed out.
FATP_TEST_CASE(revert_does_not_over_rewind_at_saturation)
{
    using NoRec = IdGenerator<uint8_t,
                              SequentialAllocationPolicy<uint8_t>,
                              NoRecyclingPolicy<uint8_t>>;
    NoRec gen(0);

    auto first = gen.generate_batch(100); // 0..99
    FATP_ASSERT_TRUE(first.has_value(), "first batch");
    std::vector<uint8_t> ids(first->begin(), first->end());
    (void)gen.release_batch(ids);

    auto second = gen.generate_batch(200); // 100..255, then Overflow -> rollback
    FATP_ASSERT_FALSE(second.has_value(), "second batch overflows");

    auto next = gen.generate();
    FATP_ASSERT_TRUE(next.has_value(), "a slot remains");
    FATP_ASSERT_EQ(*next, uint8_t(100),
                   "resumes at 100; rewinding one too far would re-issue 99");
    return true;
}

// reset() while a guard is alive must not let that guard release an ID the
// generator has since reissued to a different owner.
FATP_TEST_CASE(reset_invalidates_outstanding_guards)
{
    SimpleIdGenerator<uint64_t> gen(1);

    {
        auto held = gen.scoped_id();
        FATP_ASSERT_TRUE(held.has_value(), "guard holds an ID");
        const uint64_t guarded = held->get();
        FATP_ASSERT_EQ(gen.active_count(), size_t(1), "one active");

        gen.reset();
        FATP_ASSERT_EQ(gen.active_count(), size_t(0), "reset cleared the active set");

        // A new owner takes the same raw ID the stale guard still names.
        auto reissued = gen.generate();
        FATP_ASSERT_TRUE(reissued.has_value(), "reissued after reset");
        FATP_ASSERT_EQ(*reissued, guarded, "the same raw value is now owned by someone else");
    } // stale guard destructs here

    FATP_ASSERT_EQ(gen.active_count(), size_t(1),
                   "the stale guard must NOT have released the new owner's ID");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(0), "and must not have pooled it");
    return true;
}

// IdGuard's move-assignment release branch and operator* were never executed.
FATP_TEST_CASE(guard_move_assignment_releases_the_dropped_id)
{
    SimpleIdGenerator<uint64_t> gen(1);

    auto ha = gen.scoped_id();
    auto hb = gen.scoped_id();
    FATP_ASSERT_TRUE(ha.has_value() && hb.has_value(), "two guards");

    const uint64_t dropped = ha->get();
    const uint64_t kept = hb->get();
    FATP_ASSERT_TRUE(dropped != kept, "two distinct IDs");
    FATP_ASSERT_EQ(gen.active_count(), size_t(2), "both active");

    *ha = std::move(*hb); // ha's original ID must be released here

    FATP_ASSERT_EQ(gen.active_count(), size_t(1), "the dropped ID was released");
    FATP_ASSERT_FALSE(gen.is_active(dropped), "specifically the one ha held");
    FATP_ASSERT_TRUE(gen.is_active(kept), "and the adopted one is still live");
    FATP_ASSERT_EQ(ha->get(), kept, "the adopted ID is reported");
    FATP_ASSERT_FALSE(static_cast<bool>(*hb), "the moved-from guard is disarmed");
    return true;
}

// A StrongId generator must never issue the reserved invalid() sentinel, which
// is the underlying max(): a caller checking isValid() would read a live ID as
// absent.
FATP_TEST_CASE(strong_id_never_issues_the_invalid_sentinel)
{
    using Tag = StrongId<uint8_t, struct SentinelTag>;
    IdGenerator<Tag> gen(250);

    size_t issued = 0;
    while (true)
    {
        auto r = gen.generate();
        if (!r.has_value())
        {
            FATP_ASSERT_TRUE(r.error() == IdError::Overflow,
                             "refusing the sentinel reports Overflow");
            break;
        }
        FATP_ASSERT_TRUE(r->isValid(), "every issued StrongId is valid");
        FATP_ASSERT_TRUE(*r != Tag::invalid(), "and is never the sentinel itself");
        ++issued;
        FATP_ASSERT_TRUE(issued < 32, "loop guard");
    }

    FATP_ASSERT_EQ(issued, size_t(5), "250..254 are issuable; 255 is reserved");
    return true;
}

// Refusing the reserved sentinel must not be mistaken for exhaustion under a
// COLLIDING policy: a random draw can land on it at any time, with the space
// nearly empty. A sequential policy reaches it only at the domain's end, where
// Overflow is correct.
FATP_TEST_CASE(random_retries_the_sentinel_instead_of_reporting_overflow)
{
    using Tag = StrongId<uint8_t, struct RandomSentinelTag>;
    using RandomTag = IdGenerator<Tag,
                                  RandomAllocationPolicy<uint8_t>,
                                  NoRecyclingPolicy<uint8_t>>;
    RandomTag gen;

    // 255 is reserved, so 0..254 are issuable. Draw well past the point where
    // an unlucky sentinel draw is near-certain: an immediate Overflow on that
    // draw would stop us far short while the space was still mostly free.
    size_t issued = 0;
    for (int i = 0; i < 200; ++i)
    {
        auto r = gen.generate();
        if (!r.has_value())
        {
            break;
        }
        FATP_ASSERT_TRUE(r->isValid(), "never the sentinel");
        ++issued;
    }

    FATP_ASSERT_TRUE(issued >= 150,
                     "a sentinel draw must be retried like any other collision; "
                     "treating it as exhaustion stops generation with the space free");
    return true;
}

// The bounded policy parks its counter at the bound exactly as the unbounded one
// parks at the type maximum, so it needs the same revert correction.
FATP_TEST_CASE(bounded_revert_does_not_over_rewind_at_the_bound)
{
    using Bounded = IdGenerator<uint16_t,
                                BoundedSequentialAllocationPolicy<uint16_t>,
                                NoRecyclingPolicy<uint16_t>>;
    // The bound is a construction-time property of the policy, which the
    // generator cannot forward; exercise the policy directly.
    BoundedSequentialAllocationPolicy<uint16_t> policy(0, 4);

    std::vector<uint16_t> issued;
    bool first = true;
    uint16_t last = 0;
    while (true)
    {
        auto id = policy.next_id(last, first);
        if (!id.has_value())
        {
            break;
        }
        issued.push_back(*id);
        last = *id;
        first = false;
    }
    FATP_ASSERT_EQ(issued.size(), size_t(5), "0..4 are issuable");
    FATP_ASSERT_EQ(issued.back(), uint16_t(4), "the last is the bound itself");

    // Revert only the last TWO (4 and 3). Issuing 4 parked the counter without
    // advancing it, so the counter must come back by one, to 3 -- not by two,
    // which would re-open 2, an ID that was never reverted.
    //
    // The check must run with first_call = true (an emptied active set), because
    // otherwise next_id's max(mNextId, max_id + 1) term masks the counter's
    // value and both the correct and incorrect states answer alike.
    policy.revert(2);
    auto again = policy.next_id(0, true);
    FATP_ASSERT_TRUE(again.has_value(), "reverting reopens the range");
    FATP_ASSERT_EQ(*again, uint16_t(3),
                   "resumes at 3; rewinding one too far re-issues 2, which was never reverted");

    (void)sizeof(Bounded);
    return true;
}

// The bounded policy's upper bound is now reachable through IdGenerator. Before
// the opt-in constructor it was not: the generator forwarded base_id alone, so a
// "bounded" generator silently used the policy's default bound of max() and was
// not bounded at all.
FATP_TEST_CASE(bounded_generator_honors_its_upper_bound)
{
    using Bounded = IdGenerator<uint16_t,
                                BoundedSequentialAllocationPolicy<uint16_t>,
                                NoRecyclingPolicy<uint16_t>>;
    Bounded gen(10, 14); // inclusive: 10, 11, 12, 13, 14

    std::vector<uint16_t> issued;
    while (true)
    {
        auto r = gen.generate();
        if (!r.has_value())
        {
            FATP_ASSERT_TRUE(r.error() == IdError::Overflow, "the bound reports Overflow");
            break;
        }
        issued.push_back(*r);
        FATP_ASSERT_TRUE(issued.size() <= 16, "loop guard: the bound is not being honored");
    }

    FATP_ASSERT_EQ(issued.size(), size_t(5), "exactly 10..14 are issuable");
    FATP_ASSERT_EQ(issued.front(), uint16_t(10), "starts at the base");
    FATP_ASSERT_EQ(issued.back(), uint16_t(14), "ends at the bound, inclusive");

    // The single-argument constructor still exists and still means "unbounded"
    // for this policy, which is the pre-existing behavior.
    Bounded unbounded(10);
    auto first = unbounded.generate();
    FATP_ASSERT_TRUE(first.has_value() && *first == uint16_t(10), "one-arg form still works");
    return true;
}

// RandomAllocationPolicy accepted base_id and discarded it, so a generator
// constructed with a base produced IDs below it. It is a MINIMUM for a random
// policy, not a "first ID" -- there is no first.
FATP_TEST_CASE(random_honors_base_id_as_a_lower_bound)
{
    RandomIdGenerator<uint16_t> gen(40000);

    for (int i = 0; i < 200; ++i)
    {
        auto r = gen.generate();
        FATP_ASSERT_TRUE(r.has_value(), "space is ample above 40000");
        FATP_ASSERT_TRUE(*r >= uint16_t(40000),
                         "a random draw must not fall below the configured base");
    }

    // reset(base) must rebuild the distribution, not just reseed the engine.
    gen.reset();
    for (int i = 0; i < 50; ++i)
    {
        auto r = gen.generate();
        FATP_ASSERT_TRUE(r.has_value(), "still generating after reset");
        FATP_ASSERT_TRUE(*r >= uint16_t(40000), "the base survives reset()");
    }
    return true;
}

// generate_batch must refuse a count the domain cannot satisfy rather than
// throwing length_error/bad_alloc out of an Expected-returning API.
FATP_TEST_CASE(batch_refuses_an_impossible_count)
{
    SimpleIdGenerator<uint8_t> gen(0); // 256 IDs total

    auto absurd = gen.generate_batch(100000);
    FATP_ASSERT_FALSE(absurd.has_value(), "a count beyond the domain is refused");
    FATP_ASSERT_TRUE(absurd.error() == IdError::Overflow, "and reported as Overflow");
    FATP_ASSERT_EQ(gen.active_count(), size_t(0), "with nothing allocated");

    // Exactly the domain size is still feasible.
    auto full = gen.generate_batch(256);
    FATP_ASSERT_TRUE(full.has_value(), "the whole domain is a legal batch");
    FATP_ASSERT_EQ(gen.active_count(), size_t(256), "all issued");

    // And one more than remains is refused.
    auto over = gen.generate_batch(1);
    FATP_ASSERT_FALSE(over.has_value(), "nothing left");
    return true;
}

// StrongId coverage previously stopped at generate()/release(). The batch,
// guard, and query paths carry their own conversions.
FATP_TEST_CASE(strong_id_batch_guard_and_queries)
{
    using Tag = StrongId<uint64_t, struct BatchTag>;
    IdGenerator<Tag> gen(500);

    auto batch = gen.generate_batch(3);
    FATP_ASSERT_TRUE(batch.has_value(), "strong-id batch");
    FATP_ASSERT_EQ(batch->size(), size_t(3), "three IDs");
    FATP_ASSERT_EQ((*batch)[0].get(), uint64_t(500), "starts at the base");
    FATP_ASSERT_TRUE(gen.is_active((*batch)[0]), "is_active accepts a StrongId");
    FATP_ASSERT_EQ(gen.active_count(), size_t(3), "three active");

    {
        auto held = gen.scoped_id();
        FATP_ASSERT_TRUE(held.has_value(), "scoped StrongId");
        FATP_ASSERT_TRUE(held->get().isValid(), "guarded StrongId is valid");
        FATP_ASSERT_EQ(gen.active_count(), size_t(4), "guard holds a fourth");
    }
    FATP_ASSERT_EQ(gen.active_count(), size_t(3), "guard released it");

    auto released = gen.release_batch(*batch);
    FATP_ASSERT_TRUE(released.has_value(), "strong-id release_batch");
    FATP_ASSERT_EQ(gen.active_count(), size_t(0), "all released");
    return true;
}

// Movability is a compiler-oracle claim, not a source-reading claim. The
// shipped aliases are immovable because SingleThreadedPolicy is; the movable
// policy exists for owners that are safe to relocate.
FATP_TEST_CASE(movability_matches_the_concurrency_policy)
{
    static_assert(!std::is_move_constructible_v<SimpleIdGenerator<uint64_t>>,
                  "shipped aliases are immovable: IdGuard holds a back-pointer");
    static_assert(!std::is_move_assignable_v<SimpleIdGenerator<uint64_t>>, "same");
    static_assert(!std::is_move_constructible_v<DenseIdGenerator<uint64_t>>, "same");
    static_assert(!std::is_move_constructible_v<RandomIdGenerator<uint64_t>>, "same");

    using MovableGen = IdGenerator<uint64_t,
                                   SequentialAllocationPolicy<uint64_t>,
                                   ImmediateRecyclingPolicy<uint64_t>,
                                   id_generator::ExpectedErrorPolicy<uint64_t, IdError>,
                                   MovableSingleThreadedPolicy>;
    static_assert(std::is_move_constructible_v<MovableGen>,
                  "MovableSingleThreadedPolicy does not force immovability");
    static_assert(std::is_move_assignable_v<MovableGen>, "same");

    // A trait is not a use: exercise an actual move so the bodies instantiate.
    MovableGen a(10);
    auto first = a.generate();
    FATP_ASSERT_TRUE(first.has_value(), "generated before the move");

    MovableGen b = std::move(a);
    FATP_ASSERT_EQ(b.active_count(), size_t(1), "state travelled to the destination");
    FATP_ASSERT_TRUE(b.is_active(10), "including the active set");

    auto next = b.generate();
    FATP_ASSERT_TRUE(next.has_value() && *next == uint64_t(11),
                     "and the destination continues the sequence");
    return true;
}

// =============================================================================
// Sparse ID claiming -- SparseRecyclingPolicy
// =============================================================================

namespace
{

/// @brief The whole-domain invariant, checked wherever a test pauses.
///
/// @details free + active == domain, and credits == actives. The second is not
/// bookkeeping trivia: it is what makes release() nothrow by construction, so a
/// drift in it is a latent bad_alloc-in-a-noexcept-function, not a counter bug.
template <typename Gen>
bool sparse_invariants_hold(const Gen& gen, size_t domain_size)
{
    return gen.recycled_count() + gen.active_count() == domain_size &&
           gen.reserved_credit_count() == gen.active_count();
}

/// @brief Does this generator offer claim()? Detected by expression validity.
///
/// @details void_t rather than a requires-expression: MSVC reports the
/// constrained member's unsatisfied constraint as a hard error inside a
/// requires-expression instead of yielding false, so the requires form cannot
/// express "this member is absent".
template <typename Gen, typename = void>
struct has_claim : std::false_type
{
};

template <typename Gen>
struct has_claim<Gen,
                 std::void_t<decltype(std::declval<Gen&>().claim(
                     std::declval<typename Gen::id_type>()))>> : std::true_type
{
};

/// @brief Same detector for the two full-domain-only query methods.
///
/// @details A constrained member template is never instantiated for a generator
/// that does not call it, so dropping the `requires` clauses passes the whole
/// suite -- and then every alias advertises two queries its policy cannot answer.
template <typename Gen, typename = void>
struct has_interval_query : std::false_type
{
};

template <typename Gen>
struct has_interval_query<
    Gen,
    std::void_t<decltype(std::declval<const Gen&>().free_interval_count())>> : std::true_type
{
};

template <typename Gen, typename = void>
struct has_credit_query : std::false_type
{
};

template <typename Gen>
struct has_credit_query<
    Gen,
    std::void_t<decltype(std::declval<const Gen&>().reserved_credit_count())>> : std::true_type
{
};

} // namespace

// A claim is only meaningful at the edges too: the first identifier the
// generator would have issued anyway, and the last one it can ever issue.
FATP_TEST_CASE(sparse_claims_the_domain_endpoints)
{
    SparseIdGenerator<uint16_t> gen(0, 100);
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(101), "domain is [0, 100] inclusive");

    FATP_ASSERT_TRUE(gen.claim(0).has_value(), "the base is claimable");
    FATP_ASSERT_TRUE(gen.claim(100).has_value(), "so is the ceiling");
    FATP_ASSERT_TRUE(gen.is_active(0) && gen.is_active(100), "both are active");
    FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 101), "invariants after endpoint claims");

    // Trimming both ends leaves one interval, not three.
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1), "endpoint trims do not split");

    auto next = gen.generate();
    FATP_ASSERT_TRUE(next.has_value() && *next == uint16_t(1),
                     "generation resumes above the claimed base");
    return true;
}

// The point of the policy: reserve 60000 without paying for 0..59999.
FATP_TEST_CASE(sparse_claim_does_not_consume_lower_identifiers)
{
    SparseIdGenerator<uint16_t> gen(0, 65534);

    FATP_ASSERT_TRUE(gen.claim(60000).has_value(), "a high identifier is claimable directly");
    FATP_ASSERT_EQ(gen.active_count(), size_t(1), "exactly one identifier was consumed");
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(2), "an interior claim splits one interval");

    auto first = gen.generate();
    FATP_ASSERT_TRUE(first.has_value() && *first == uint16_t(0),
                     "everything below the claim is still free");
    FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 65535), "invariants after a sparse claim");
    return true;
}

// Claim order must not matter: the resulting state is a set, not a history.
FATP_TEST_CASE(sparse_claim_order_does_not_change_the_result)
{
    const std::array<std::array<uint16_t, 3>, 3> orders = {
        std::array<uint16_t, 3>{0, 5, 60000},
        std::array<uint16_t, 3>{60000, 5, 0},
        std::array<uint16_t, 3>{5, 60000, 0}};

    for (const auto& order : orders)
    {
        SparseIdGenerator<uint16_t> gen(0, 65534);
        for (uint16_t id : order)
        {
            FATP_ASSERT_TRUE(gen.claim(id).has_value(), "claim succeeds in any order");
        }

        FATP_ASSERT_EQ(gen.active_count(), size_t(3), "three identifiers claimed");
        FATP_ASSERT_EQ(gen.free_interval_count(), size_t(3),
                       "0 trims the front, 5 and 60000 each split: three intervals");
        FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 65535), "invariants regardless of order");

        auto next = gen.generate();
        FATP_ASSERT_TRUE(next.has_value() && *next == uint16_t(1),
                         "and issuance resumes at the same place");
    }
    return true;
}

// A duplicate claim is AlreadyInUse -- the caller named an ACTIVE identifier --
// and must leave every observable untouched, including issuance order.
FATP_TEST_CASE(sparse_duplicate_claim_is_refused_without_side_effects)
{
    SparseIdGenerator<uint16_t> gen(0, 9);
    FATP_ASSERT_TRUE(gen.claim(4).has_value(), "first claim");

    const size_t intervals = gen.free_interval_count();
    const size_t free_count = gen.recycled_count();

    auto again = gen.claim(4);
    FATP_ASSERT_TRUE(!again.has_value(), "the duplicate is refused");
    FATP_ASSERT_TRUE(again.error() == IdError::AlreadyInUse, "as AlreadyInUse, not InvalidClaim");

    FATP_ASSERT_EQ(gen.free_interval_count(), intervals, "interval count unchanged");
    FATP_ASSERT_EQ(gen.recycled_count(), free_count, "free count unchanged");
    FATP_ASSERT_EQ(gen.active_count(), size_t(1), "active count unchanged");
    FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 10), "no credit leaked by the refusal");

    auto next = gen.generate();
    FATP_ASSERT_TRUE(next.has_value() && *next == uint16_t(0), "issuance order unchanged");
    return true;
}

// An out-of-domain claim is a DOMAIN error, distinct from naming an active
// identifier, and it too must mutate nothing.
FATP_TEST_CASE(sparse_out_of_domain_claim_is_invalid_claim)
{
    SparseIdGenerator<uint16_t> gen(10, 20);

    const size_t intervals = gen.free_interval_count();
    const size_t free_count = gen.recycled_count();

    auto below = gen.claim(9);
    FATP_ASSERT_TRUE(!below.has_value() && below.error() == IdError::InvalidClaim,
                     "below the base is InvalidClaim");

    auto above = gen.claim(21);
    FATP_ASSERT_TRUE(!above.has_value() && above.error() == IdError::InvalidClaim,
                     "above the ceiling is InvalidClaim");

    FATP_ASSERT_EQ(gen.free_interval_count(), intervals, "no interval was disturbed");
    FATP_ASSERT_EQ(gen.recycled_count(), free_count, "no identifier was consumed");
    FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 11), "no credit reserved by a refusal");
    return true;
}

// The reserved sentinel is excluded by DOMAIN CONSTRUCTION, not by a late
// guard, so it is out of domain rather than merely unissuable.
FATP_TEST_CASE(sparse_domain_excludes_the_reserved_sentinel)
{
    using Tag = StrongId<uint8_t, struct SparseSentinelTag>;
    SparseIdGenerator<Tag> gen(250);

    // [250, 254]: 255 is Tag::invalid() and is not part of the domain at all.
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(5), "the sentinel is not counted as free");

    auto claimed = gen.claim(Tag::invalid());
    FATP_ASSERT_TRUE(!claimed.has_value() && claimed.error() == IdError::InvalidClaim,
                     "the sentinel is outside the domain, not merely unissuable");

    size_t issued = 0;
    while (gen.generate().has_value())
    {
        ++issued;
    }
    FATP_ASSERT_EQ(issued, size_t(5), "exactly the five issuable identifiers");
    return true;
}

// Release then re-claim, both from a fresh identifier and from one that is
// sitting inside a free interval after its release.
FATP_TEST_CASE(sparse_release_then_reclaim)
{
    SparseIdGenerator<uint16_t> gen(0, 9);

    FATP_ASSERT_TRUE(gen.claim(5).has_value(), "claimed");
    FATP_ASSERT_TRUE(gen.release(5).has_value(), "released");
    FATP_ASSERT_TRUE(!gen.is_active(5), "no longer active");
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1), "release merged the domain back to one");
    FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 10), "invariants after release");

    FATP_ASSERT_TRUE(gen.claim(5).has_value(), "an identifier inside a free interval reclaims");
    FATP_ASSERT_TRUE(gen.is_active(5), "and is active again");

    auto twice = gen.release(5);
    FATP_ASSERT_TRUE(twice.has_value(), "the single release succeeds");
    auto duplicate = gen.release(5);
    FATP_ASSERT_TRUE(!duplicate.has_value() && duplicate.error() == IdError::InvalidRelease,
                     "the duplicate release is refused");
    FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 10), "a refused release consumes no credit");
    return true;
}

// The four release transitions, each asserted on the INTERVAL COUNT. Membership
// and totals are identical whether or not adjacent intervals were merged, so
// canonicalization needs its own oracle.
FATP_TEST_CASE(sparse_release_transitions_canonicalize)
{
    // (4) Singleton insertion: neither neighbour is free.
    {
        SparseIdGenerator<uint16_t> gen(0, 9);
        for (uint16_t id : std::array<uint16_t, 3>{3, 4, 5})
        {
            FATP_ASSERT_TRUE(gen.claim(id).has_value(), "claim");
        }
        FATP_ASSERT_EQ(gen.free_interval_count(), size_t(2), "free is [0,2] and [6,9]");

        FATP_ASSERT_TRUE(gen.release(4).has_value(), "release the middle of an active run");
        FATP_ASSERT_EQ(gen.free_interval_count(), size_t(3), "a singleton [4,4] appears");
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(8), "3 + 1 + 4 free");
        FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 10), "invariants");
    }

    // (3) Extend the right neighbour's lower bound.
    {
        SparseIdGenerator<uint16_t> gen(0, 9);
        for (uint16_t id : std::array<uint16_t, 3>{3, 4, 5})
        {
            FATP_ASSERT_TRUE(gen.claim(id).has_value(), "claim");
        }
        FATP_ASSERT_TRUE(gen.release(5).has_value(), "release adjacent to [6,9] on the right");
        FATP_ASSERT_EQ(gen.free_interval_count(), size_t(2), "absorbed, not appended");
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(8), "[0,2] and [5,9]");
    }

    // (2) Extend the left neighbour's upper bound -- the rekeying transition.
    {
        SparseIdGenerator<uint16_t> gen(0, 9);
        for (uint16_t id : std::array<uint16_t, 3>{3, 4, 5})
        {
            FATP_ASSERT_TRUE(gen.claim(id).has_value(), "claim");
        }
        FATP_ASSERT_TRUE(gen.release(3).has_value(), "release adjacent to [0,2] on the left");
        FATP_ASSERT_EQ(gen.free_interval_count(), size_t(2), "absorbed, not appended");
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(8), "[0,3] and [6,9]");
    }

    // (1) Merge both neighbours through the released identifier.
    {
        SparseIdGenerator<uint16_t> gen(0, 9);
        for (uint16_t id : std::array<uint16_t, 3>{3, 4, 5})
        {
            FATP_ASSERT_TRUE(gen.claim(id).has_value(), "claim");
        }
        FATP_ASSERT_TRUE(gen.release(3).has_value(), "left");
        FATP_ASSERT_TRUE(gen.release(5).has_value(), "right");
        FATP_ASSERT_EQ(gen.free_interval_count(), size_t(2), "[0,3] and [5,9]");

        FATP_ASSERT_TRUE(gen.release(4).has_value(), "close the gap between them");
        FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1),
                       "one interval, not two adjacent ones");
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(10), "the whole domain is free again");
        FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 10), "invariants");
    }
    return true;
}

// Generation always returns the lowest FREE identifier, which sparse claims
// move around.
FATP_TEST_CASE(sparse_generation_returns_the_lowest_free_identifier)
{
    SparseIdGenerator<uint16_t> gen(0, 9);
    for (uint16_t id : std::array<uint16_t, 3>{0, 1, 3})
    {
        FATP_ASSERT_TRUE(gen.claim(id).has_value(), "claim");
    }

    auto a = gen.generate();
    FATP_ASSERT_TRUE(a.has_value() && *a == uint16_t(2), "2 is the lowest free");
    auto b = gen.generate();
    FATP_ASSERT_TRUE(b.has_value() && *b == uint16_t(4), "then 4, skipping the claims");
    FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 10), "invariants");
    return true;
}

// A batch never issues a claimed identifier, and a batch that cannot complete
// restores the policy EXACTLY -- interval structure included, not just counts.
FATP_TEST_CASE(sparse_batch_excludes_claims_and_rolls_back_exactly)
{
    SparseIdGenerator<uint16_t> gen(0, 9);
    FATP_ASSERT_TRUE(gen.claim(2).has_value() && gen.claim(7).has_value(), "two claims");

    auto batch = gen.generate_batch(3);
    FATP_ASSERT_TRUE(batch.has_value(), "a feasible batch succeeds");
    FATP_ASSERT_EQ(batch->size(), size_t(3), "three identifiers");
    FATP_ASSERT_TRUE((*batch)[0] == uint16_t(0) && (*batch)[1] == uint16_t(1) &&
                         (*batch)[2] == uint16_t(3),
                     "the claims are skipped");
    FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 10), "invariants after a batch");

    // uint64_t so the size_t capacity preflight is compiled out and the batch
    // must actually run out mid-loop -- which is the path that rolls back.
    SparseIdGenerator<uint64_t> wide(0, 9);
    FATP_ASSERT_TRUE(wide.claim(2).has_value() && wide.claim(7).has_value(), "same two claims");

    const size_t intervals = wide.free_interval_count();
    const size_t free_count = wide.recycled_count();

    auto doomed = wide.generate_batch(9);
    FATP_ASSERT_TRUE(!doomed.has_value() && doomed.error() == IdError::Overflow,
                     "only 8 are free, so the batch is refused");
    FATP_ASSERT_EQ(wide.free_interval_count(), intervals, "interval STRUCTURE is restored");
    FATP_ASSERT_EQ(wide.recycled_count(), free_count, "and so is the free count");
    FATP_ASSERT_EQ(wide.active_count(), size_t(2), "only the claims remain active");
    FATP_ASSERT_TRUE(sparse_invariants_hold(wide, 10), "rollback consumed exactly its credits");
    return true;
}

// Exhaustion is an EMPTY FREE SET, reported without consulting the allocation
// policy -- whose own exhaustion point is far above the configured ceiling.
FATP_TEST_CASE(sparse_exhaustion_is_the_configured_ceiling)
{
    SparseIdGenerator<uint8_t> gen(0, 3);

    for (int i = 0; i < 4; ++i)
    {
        FATP_ASSERT_TRUE(gen.generate().has_value(), "the domain holds four identifiers");
    }
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(0), "nothing free");
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(0), "exhaustion IS an empty map");

    auto over = gen.generate();
    FATP_ASSERT_TRUE(!over.has_value() && over.error() == IdError::Overflow,
                     "exhausted at the configured ceiling, not at uint8_t's maximum");

    auto claimed = gen.claim(2);
    FATP_ASSERT_TRUE(!claimed.has_value() && claimed.error() == IdError::AlreadyInUse,
                     "and every identifier in it is active");

    // Releasing one makes exactly one available again.
    FATP_ASSERT_TRUE(gen.release(1).has_value(), "release");
    auto reissued = gen.generate();
    FATP_ASSERT_TRUE(reissued.has_value() && *reissued == uint8_t(1), "reissued");
    return true;
}

// recycled_count() means "how many are free", and it must never wrap. The
// full-width domain has cardinality 2^64, which is not representable.
FATP_TEST_CASE(sparse_recycled_count_is_exact_then_saturates)
{
    {
        SparseIdGenerator<uint16_t> narrow(0, 9);
        FATP_ASSERT_EQ(narrow.recycled_count(), size_t(10), "exact when representable");
        FATP_ASSERT_TRUE(narrow.claim(5).has_value(), "claim splits it");
        FATP_ASSERT_EQ(narrow.recycled_count(), size_t(9), "still exact across two intervals");
    }

    SparseIdGenerator<uint64_t> full(0);
    constexpr size_t kMax = std::numeric_limits<size_t>::max();

    // [0, 2^64-1] holds 2^64 identifiers: unrepresentable, so it saturates.
    // The failure this catches is `upper - lower + 1` wrapping to ZERO, which
    // would report a full domain as exhausted.
    FATP_ASSERT_EQ(full.recycled_count(), kMax, "the full-width domain saturates, never wraps");
    FATP_ASSERT_TRUE(full.recycled_count() != size_t(0), "and specifically is not zero");

    auto first = full.generate();
    FATP_ASSERT_TRUE(first.has_value() && *first == uint64_t(0), "issue one");
    FATP_ASSERT_EQ(full.recycled_count(), kMax, "2^64-1 is exactly SIZE_MAX");

    // Both assertions above are satisfied by a SATURATING answer as well as an
    // exact one, so on their own they leave the threshold free to move by 2^63.
    // This is the assertion a saturating answer cannot also satisfy.
    auto second = full.generate();
    FATP_ASSERT_TRUE(second.has_value() && *second == uint64_t(1), "issue another");
    FATP_ASSERT_EQ(full.recycled_count(), kMax - 1, "exact one below the representable ceiling");

    auto third = full.generate();
    FATP_ASSERT_TRUE(third.has_value() && *third == uint64_t(2), "and another");
    FATP_ASSERT_EQ(full.recycled_count(), kMax - 2, "still exact");

    // Two intervals summing at operand sizes nothing else in the suite reaches,
    // which is the cross-interval accumulation path rather than the per-term one.
    FATP_ASSERT_TRUE(full.release(1).has_value(), "release the middle -> [1,1] and [3, 2^64-1]");
    FATP_ASSERT_EQ(full.free_interval_count(), size_t(2), "two intervals");
    FATP_ASSERT_EQ(full.recycled_count(), kMax - 1, "1 + (2^64-3), summed without wrapping");
    return true;
}

// reset() must REBUILD the domain. A clear()-only reset leaves an empty map,
// which reads as exhausted -- so this asserts by generating the base again, not
// merely by is_active() being false.
FATP_TEST_CASE(sparse_reset_rebuilds_the_configured_domain)
{
    SparseIdGenerator<uint16_t> gen(100, 200);

    auto first = gen.generate();
    FATP_ASSERT_TRUE(first.has_value() && *first == uint16_t(100), "a non-zero base issues first");
    FATP_ASSERT_TRUE(gen.claim(150).has_value(), "and a sparse claim lands");
    FATP_ASSERT_TRUE(gen.is_active(150), "active before the reset");

    gen.reset();

    FATP_ASSERT_EQ(gen.active_count(), size_t(0), "every prior claim is inactive");
    FATP_ASSERT_TRUE(!gen.is_active(150), "including the sparse one");
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1), "the domain is one interval again");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(101), "of exactly [100, 200]");
    FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 101), "no credits survive the reset");

    auto again = gen.generate();
    FATP_ASSERT_TRUE(again.has_value() && *again == uint16_t(100),
                     "and it issues the base again -- a clear()-only reset would report Overflow");
    FATP_ASSERT_TRUE(gen.claim(150).has_value(), "the sparse identifier is claimable again");
    return true;
}

// The exhausted reset is the case the return credits exist for: an empty map
// with no node to reuse, funded by a credit belonging to a still-active id.
FATP_TEST_CASE(sparse_reset_from_exhaustion_reuses_a_credit)
{
    SparseIdGenerator<uint8_t> gen(0, 3);
    for (int i = 0; i < 4; ++i)
    {
        FATP_ASSERT_TRUE(gen.generate().has_value(), "exhaust the domain");
    }
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(0), "no node left to reuse");
    FATP_ASSERT_EQ(gen.reserved_credit_count(), size_t(4), "but four credits are held");

    gen.reset();

    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1), "the domain was rebuilt from a credit");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(4), "as the whole configured domain");
    FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 4), "and the remaining credits went with it");
    return true;
}

// The same contract under a real lock.
FATP_TEST_CASE(sparse_contract_holds_under_mutex_synchronization)
{
    ThreadSafeSparseIdGenerator<uint16_t> gen(0, 9);

    FATP_ASSERT_TRUE(gen.claim(7).has_value(), "claim");
    auto duplicate = gen.claim(7);
    FATP_ASSERT_TRUE(!duplicate.has_value() && duplicate.error() == IdError::AlreadyInUse,
                     "duplicate refused");
    auto outside = gen.claim(10);
    FATP_ASSERT_TRUE(!outside.has_value() && outside.error() == IdError::InvalidClaim,
                     "out of domain refused");

    auto issued = gen.generate();
    FATP_ASSERT_TRUE(issued.has_value() && *issued == uint16_t(0), "generation unaffected");
    FATP_ASSERT_TRUE(gen.release(7).has_value(), "release");
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1), "and it merges back");
    FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 10), "invariants under the lock");
    return true;
}

// Nothing the existing aliases do changes. They get no claim member, no
// ordering change, and the same errors.
FATP_TEST_CASE(sparse_policy_does_not_reach_the_existing_aliases)
{
    static_assert(!has_claim<SimpleIdGenerator<uint64_t>>::value,
                  "claim() is exclusive to a full-domain recycling policy");
    static_assert(!has_claim<DenseIdGenerator<uint64_t>>::value, "same");
    static_assert(!has_claim<RandomIdGenerator<uint64_t>>::value, "same");
    static_assert(has_claim<SparseIdGenerator<uint64_t>>::value,
                  "and is present on the sparse alias");
    static_assert(has_claim<ThreadSafeSparseIdGenerator<uint64_t>>::value, "and its thread-safe twin");

    // The two full-domain query methods are gated the same way.
    static_assert(!has_interval_query<SimpleIdGenerator<uint64_t>>::value,
                  "free_interval_count() is meaningless without an interval set");
    static_assert(!has_interval_query<DenseIdGenerator<uint64_t>>::value, "same");
    static_assert(!has_interval_query<RandomIdGenerator<uint64_t>>::value, "same");
    static_assert(has_interval_query<SparseIdGenerator<uint64_t>>::value, "present where it means something");
    static_assert(has_interval_query<ThreadSafeSparseIdGenerator<uint64_t>>::value, "and there");

    static_assert(!has_credit_query<SimpleIdGenerator<uint64_t>>::value,
                  "reserved_credit_count() is meaningless without return credits");
    static_assert(!has_credit_query<DenseIdGenerator<uint64_t>>::value, "same");
    static_assert(!has_credit_query<RandomIdGenerator<uint64_t>>::value, "same");
    static_assert(has_credit_query<SparseIdGenerator<uint64_t>>::value, "present where it means something");
    static_assert(has_credit_query<ThreadSafeSparseIdGenerator<uint64_t>>::value, "and there");

    // reset() stays unconditionally noexcept for the shipping policies and is
    // conditionally noexcept only where a domain rebuild may allocate.
    static_assert(noexcept(std::declval<SimpleIdGenerator<uint64_t>&>().reset()),
                  "the shipping policies keep the noexcept they ship with");
    static_assert(!noexcept(std::declval<SparseIdGenerator<uint64_t>&>().reset()),
                  "the sparse rebuild is honest about the moved-from allocation");

    SimpleIdGenerator<uint64_t> fifo;
    FATP_ASSERT_TRUE(*fifo.generate() == uint64_t(0), "0");
    FATP_ASSERT_TRUE(*fifo.generate() == uint64_t(1), "1");
    FATP_ASSERT_TRUE(*fifo.generate() == uint64_t(2), "2");
    FATP_ASSERT_TRUE(fifo.release(1).has_value(), "release the middle");
    FATP_ASSERT_TRUE(*fifo.generate() == uint64_t(1), "FIFO recycling is unchanged");

    DenseIdGenerator<uint64_t> dense;
    for (int i = 0; i < 3; ++i)
    {
        (void)dense.generate();
    }
    FATP_ASSERT_TRUE(dense.release(2).has_value() && dense.release(0).has_value(), "release two");
    FATP_ASSERT_TRUE(*dense.generate() == uint64_t(0), "min-first recycling is unchanged");

    SimpleIdGenerator<uint8_t> small(250);
    for (int i = 0; i < 6; ++i)
    {
        FATP_ASSERT_TRUE(small.generate().has_value(), "250..255");
    }
    auto over = small.generate();
    FATP_ASSERT_TRUE(!over.has_value() && over.error() == IdError::Overflow,
                     "and the same Overflow at the same place");
    return true;
}

namespace
{

/// @brief The sparse policy with an injectable allocation failure.
///
/// @details The staged order in generate() -- peek, reserve the credit, insert
/// into the active set, and only THEN remove from the free set -- is invisible
/// to every test that does not make one of those steps fail. Reordering it
/// passes the entire suite otherwise, which makes the ordering claim unfalsified
/// rather than verified.
///
/// The generator is a template over its recycling policy, so the injection point
/// costs the shipped code nothing: this derived policy shadows `reserve_credit`,
/// and IdGenerator's qualified call resolves to the shadow.
template <typename IdType>
class FlakySparsePolicy : public SparseRecyclingPolicy<IdType>
{
public:
    using value_type = IdType;
    using is_full_domain_policy = void;

    static inline bool fail_next_reserve = false;

    /// @brief Fail the Nth reservation since `reserve_calls` was last zeroed.
    ///
    /// @details A batch reserves once per element, so failing "the next one"
    /// can only ever break the first. Failing the fourth is what puts a
    /// half-built batch on the rollback path.
    static inline int fail_at_reserve = -1;
    static inline int reserve_calls = 0;

    static void arm(int nth)
    {
        reserve_calls = 0;
        fail_at_reserve = nth;
        fail_next_reserve = false;
    }

    static void disarm()
    {
        fail_at_reserve = -1;
        fail_next_reserve = false;
        reserve_calls = 0;
    }

    void reserve_credit()
    {
        check_injection();
        SparseRecyclingPolicy<IdType>::reserve_credit();
    }

    // claim() reserves through this entry point, and the base's own call to
    // reserve_credit() is non-virtual, so the shadow above never sees it.
    std::size_t reserve_claim_credits(IdType id)
    {
        check_injection();
        return SparseRecyclingPolicy<IdType>::reserve_claim_credits(id);
    }

private:
    static void check_injection()
    {
        const int n = reserve_calls++;
        if (fail_next_reserve)
        {
            fail_next_reserve = false;
            throw std::bad_alloc();
        }
        if (n == fail_at_reserve)
        {
            fail_at_reserve = -1;
            throw std::bad_alloc();
        }
    }
};

/// @brief Counts every ordering comparison the interval map performs.
///
/// @details The `ascending_order` marker is the policy's opt-in: a comparator
/// must declare that it orders ascending, so an instrument can be supplied
/// without opening the door to one that would silently break the representation.
template <typename T>
struct CountingLess
{
    using ascending_order = void;

    static inline size_t comparisons = 0;

    bool operator()(const T& a, const T& b) const
    {
        ++comparisons;
        return a < b;
    }
};

template <typename IdType>
using CountingSparseGenerator =
    IdGenerator<IdType,
                SequentialAllocationPolicy<IdType>,
                SparseRecyclingPolicy<IdType, CountingLess<IdType>>,
                id_generator::ExpectedErrorPolicy<IdType, IdError>,
                SingleThreadedPolicy>;

/// @brief A sequential allocation policy that records whether revert() ran, and
/// that can run out INDEPENDENTLY of the generator's configured domain.
///
/// @details The full-domain path must never revert the allocation policy, which
/// issued nothing. `is_sequential_policy` is declared so the pairing check
/// accepts it -- the check is a property, not a whitelist of the two shipped
/// policies.
///
/// The issue cap exists because the batch capacity preflight is exact for any
/// domain narrower than SIZE_MAX+1, so domain exhaustion is now always refused
/// BEFORE the loop and can no longer reach rollback. An allocation policy that
/// stops early while the generator's configured domain says otherwise is the
/// remaining way to exercise the ordinary rollback path -- and it is the honest
/// one, since it isolates "the allocator ran out" from "the domain ran out",
/// which is exactly the distinction the sparse assertion is about.
template <typename IdType>
class CountingRevertPolicy : public SequentialAllocationPolicy<IdType>
{
public:
    using is_sequential_policy = void;

    static inline size_t reverts = 0;
    static inline size_t issue_cap = 0; // 0 = unlimited

    explicit CountingRevertPolicy(IdType base_id = 0)
        : SequentialAllocationPolicy<IdType>(base_id)
    {
    }

    std::optional<IdType> next_id(IdType max_id, bool first_call) noexcept
    {
        if (issue_cap != 0 && mIssued >= issue_cap)
        {
            return std::nullopt;
        }
        auto result = SequentialAllocationPolicy<IdType>::next_id(max_id, first_call);
        if (result)
        {
            ++mIssued;
        }
        return result;
    }

    void revert(size_t count) noexcept
    {
        ++reverts;
        mIssued = (count > mIssued) ? 0 : mIssued - count;
        SequentialAllocationPolicy<IdType>::revert(count);
    }

    void reset(IdType base_id = 0) noexcept
    {
        mIssued = 0;
        SequentialAllocationPolicy<IdType>::reset(base_id);
    }

private:
    size_t mIssued = 0;
};

/// @brief TU-level allocation counter.
///
/// @details Every existing oracle in this file is a COUNT --
/// reserved_credit_count(), free_interval_count(), recycled_count() -- and none
/// of them can tell a node taken from the credit stack from one freshly
/// allocated: both decrement the count and leave identical structure. So the
/// entire promise of the return-credit mechanism ("release is nothrow BY
/// CONSTRUCTION, not because termination hides a bad_alloc") had no witness at
/// all, while only its arithmetic was tested.
///
/// Counting is off by default, so the cost to every other test is one relaxed
/// atomic load. It is atomic rather than plain because the concurrency tests in
/// this file allocate on several threads through this same replacement.
} // namespace (the replaced operator new must see these at EXTERNAL linkage)

namespace allocprobe
{

std::atomic<bool> counting{false};
std::atomic<size_t> allocations{0};
std::atomic<long long> fail_at{-1}; // index within the counting window

inline void start(long long fail_index = -1)
{
    allocations.store(0, std::memory_order_relaxed);
    fail_at.store(fail_index, std::memory_order_relaxed);
    counting.store(true, std::memory_order_relaxed);
}

inline size_t stop()
{
    counting.store(false, std::memory_order_relaxed);
    fail_at.store(-1, std::memory_order_relaxed);
    return allocations.load(std::memory_order_relaxed);
}

/// @brief RAII window, so an injected throw cannot leave counting armed.
class Window
{
public:
    explicit Window(long long fail_index = -1)
    {
        start(fail_index);
    }
    ~Window()
    {
        (void)stop();
    }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    size_t count() const
    {
        return allocations.load(std::memory_order_relaxed);
    }
};

} // namespace allocprobe

namespace
{

template <typename IdType>
using FlakyGenerator = IdGenerator<IdType,
                                   SequentialAllocationPolicy<IdType>,
                                   FlakySparsePolicy<IdType>,
                                   id_generator::ExpectedErrorPolicy<IdType, IdError>,
                                   SingleThreadedPolicy>;

/// @brief An identifier type whose constructor throws on demand.
///
/// @details generate_batch() constructs the caller's value before the active-set
/// insert precisely so a throwing constructor cannot strand an identifier. That
/// is the other ordering claim no ordinary test can reach.
struct ThrowingId
{
    using underlying_type = uint16_t;

    static inline int constructed = 0;
    static inline int throw_at = -1;

    uint16_t v{};

    ThrowingId() = default;

    explicit ThrowingId(uint16_t raw)
        : v(raw)
    {
        const int n = constructed++;
        if (throw_at >= 0 && n == throw_at)
        {
            throw std::runtime_error("id_type constructor");
        }
    }

    uint16_t get() const noexcept
    {
        return v;
    }

    bool operator==(const ThrowingId&) const = default;
};

using ThrowingGenerator = IdGenerator<ThrowingId,
                                      SequentialAllocationPolicy<uint16_t>,
                                      SparseRecyclingPolicy<uint16_t>,
                                      id_generator::ExpectedErrorPolicy<ThrowingId, IdError>,
                                      SingleThreadedPolicy>;

} // namespace

// A failure to reserve the return credit must leave the generator EXACTLY as it
// was: the credit is reserved before the free set is touched, so there is no
// state to unwind. If the removal ran first, this failure would delete an
// identifier from the free set that never became active -- and since exhaustion
// is an empty map, nothing would ever notice it was gone.
FATP_TEST_CASE(sparse_credit_failure_leaves_the_free_set_untouched)
{
    FlakyGenerator<uint16_t> gen(0, 9);
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(10), "ten free to begin with");

    FlakySparsePolicy<uint16_t>::fail_next_reserve = true;

    bool threw = false;
    try
    {
        (void)gen.generate();
    }
    catch (const std::bad_alloc&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "the allocation failure propagates rather than becoming an IdError");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(10), "no identifier left the free set");
    FATP_ASSERT_EQ(gen.active_count(), size_t(0), "and none became active");
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1), "the domain is one interval, intact");
    FATP_ASSERT_EQ(gen.reserved_credit_count(), size_t(0), "no credit was left behind");

    auto after = gen.generate();
    FATP_ASSERT_TRUE(after.has_value() && *after == uint16_t(0),
                     "and the generator still issues the identifier it was about to lose");
    return true;
}

// The same guarantee one level up: a throw partway through a batch returns every
// identifier the batch had taken, restoring the interval STRUCTURE, not merely
// the counts -- and consuming exactly the credits it reserved.
FATP_TEST_CASE(sparse_batch_unwinds_a_throwing_id_constructor)
{
    ThrowingGenerator gen(0, 9);
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(10), "ten free");

    ThrowingId::constructed = 0;
    ThrowingId::throw_at = 3; // the fourth element of the batch

    bool threw = false;
    try
    {
        (void)gen.generate_batch(6);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    ThrowingId::throw_at = -1;

    FATP_ASSERT_TRUE(threw, "the constructor's exception reaches the caller");
    FATP_ASSERT_EQ(gen.active_count(), size_t(0), "no identifier stayed active");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(10), "the whole domain is free again");
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1),
                   "rebuilt as ONE interval -- rollback canonicalizes, it does not fragment");
    FATP_ASSERT_EQ(gen.reserved_credit_count(), size_t(0), "every credit was consumed or dropped");

    auto batch = gen.generate_batch(4);
    FATP_ASSERT_TRUE(batch.has_value(), "and the generator is fully usable afterwards");
    FATP_ASSERT_TRUE((*batch)[0] == ThrowingId(0) && (*batch)[3] == ThrowingId(3),
                     "issuing from the start of the restored domain");
    return true;
}

// claim() reserves before it mutates, so a failed reservation is invisible.
FATP_TEST_CASE(sparse_claim_credit_failure_changes_nothing)
{
    FlakyGenerator<uint16_t> gen(0, 9);
    FATP_ASSERT_TRUE(gen.claim(3).has_value(), "an ordinary claim, to fragment the domain");
    const size_t intervals = gen.free_interval_count();

    FlakySparsePolicy<uint16_t>::fail_next_reserve = true;

    bool threw = false;
    try
    {
        (void)gen.claim(7); // an interior claim: the split path, two credits
    }
    catch (const std::bad_alloc&)
    {
        threw = true;
    }

    FATP_ASSERT_TRUE(threw, "the reservation failure propagates");
    FATP_ASSERT_EQ(gen.free_interval_count(), intervals, "no interval was split");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(9), "no identifier left the free set");
    FATP_ASSERT_TRUE(!gen.is_active(7), "and 7 did not become active");
    FATP_ASSERT_EQ(gen.reserved_credit_count(), gen.active_count(), "credits still match actives");

    FATP_ASSERT_TRUE(gen.claim(7).has_value(), "and the same claim succeeds on retry");
    return true;
}

// Gap independence, instrumented. Without a comparison counter nothing separates
// claim() from the generate-and-release walk it replaces: that walk also leaves
// one identifier active and everything below it free, so counts and membership
// look identical. The comparator is the only witness.
FATP_TEST_CASE(sparse_claim_cost_is_independent_of_the_gap)
{
    size_t near_cost = 0;
    size_t far_cost = 0;

    {
        CountingSparseGenerator<uint32_t> gen(0, 2000000);
        CountingLess<uint32_t>::comparisons = 0;
        FATP_ASSERT_TRUE(gen.claim(5).has_value(), "a claim five above the base");
        near_cost = CountingLess<uint32_t>::comparisons;
    }
    {
        CountingSparseGenerator<uint32_t> gen(0, 2000000);
        CountingLess<uint32_t>::comparisons = 0;
        FATP_ASSERT_TRUE(gen.claim(1999999).has_value(), "and one two million above it");
        far_cost = CountingLess<uint32_t>::comparisons;
    }

    FATP_ASSERT_TRUE(near_cost > 0, "the comparator is actually consulted");
    FATP_ASSERT_EQ(far_cost, near_cost, "the same interval structure costs the same to claim");
    FATP_ASSERT_TRUE(far_cost < size_t(100),
                     "and it is a constant, not a walk over two million identifiers");

    // Both measurements above run on a freshly built generator holding ONE
    // interval, so I is fixed and only the gap varies -- which an O(I) claim()
    // would also pass. The bound is on INTERVAL COUNT, so it needs a fragmented
    // domain to mean anything.
    {
        CountingSparseGenerator<uint32_t> gen(0, 4000);
        for (uint32_t id = 0; id <= 4000; id += 2)
        {
            FATP_ASSERT_TRUE(gen.claim(id).has_value(), "claim every even identifier");
        }
        FATP_ASSERT_EQ(gen.free_interval_count(), size_t(2000), "leaving 2000 singleton gaps");

        CountingLess<uint32_t>::comparisons = 0;
        FATP_ASSERT_TRUE(gen.claim(3999).has_value(), "claim near the top of a fragmented domain");
        const size_t frag_cost = CountingLess<uint32_t>::comparisons;

        FATP_ASSERT_TRUE(frag_cost > near_cost, "the measurement is live at I = 2000");
        FATP_ASSERT_TRUE(frag_cost < size_t(150),
                         "but logarithmic in I -- a linear scan would record 2000+");

        // While the comparator is mounted: generate() is O(1) in I and release()
        // is O(log I). Neither was instrumented at all.
        CountingLess<uint32_t>::comparisons = 0;
        FATP_ASSERT_TRUE(gen.generate().has_value(), "generate from a 2000-interval domain");
        FATP_ASSERT_TRUE(CountingLess<uint32_t>::comparisons < size_t(20),
                         "generate() reads begin(), so it does not scale with I");

        CountingLess<uint32_t>::comparisons = 0;
        FATP_ASSERT_TRUE(gen.release(3999).has_value(), "release into a 2000-interval domain");
        FATP_ASSERT_TRUE(CountingLess<uint32_t>::comparisons < size_t(150),
                         "release() is a find plus an upper_bound, not a scan");
    }

    // The absolute budgets are toolchain-dependent, and a counting comparator
    // witnesses only comparator-mediated work: an implementation that walked
    // mFree with built-in operators would stay invisible to it.
    return true;
}

// The allocation policy issued nothing on the full-domain path, so rolling a
// batch back must not rewind it. A zero from a counter that never increments
// proves nothing, so the same policy is checked under a non-sparse generator,
// where the revert IS expected.
FATP_TEST_CASE(sparse_batch_rollback_does_not_revert_the_allocation_policy)
{
    using SparseGen = IdGenerator<uint64_t,
                                  CountingRevertPolicy<uint64_t>,
                                  SparseRecyclingPolicy<uint64_t>,
                                  id_generator::ExpectedErrorPolicy<uint64_t, IdError>,
                                  SingleThreadedPolicy>;

    // Domain exhaustion can no longer reach rollback: the capacity preflight is
    // exact for any domain narrower than SIZE_MAX+1, so an infeasible count is
    // refused before the loop. Reaching the ROLLBACK path therefore means
    // failing mid-batch, which is what the injected reservation failure does.
    using FlakySparseGen = IdGenerator<uint16_t,
                                       CountingRevertPolicy<uint16_t>,
                                       FlakySparsePolicy<uint16_t>,
                                       id_generator::ExpectedErrorPolicy<uint16_t, IdError>,
                                       SingleThreadedPolicy>;

    FlakySparseGen sparse(0, 9);
    FATP_ASSERT_TRUE(sparse.claim(4).has_value(), "one claim, so the setup is explicit");

    CountingRevertPolicy<uint16_t>::reverts = 0;
    FlakySparsePolicy<uint16_t>::arm(3); // fail the fourth element's reservation

    bool threw = false;
    try
    {
        (void)sparse.generate_batch(6);
    }
    catch (const std::bad_alloc&)
    {
        threw = true;
    }
    FlakySparsePolicy<uint16_t>::disarm();

    FATP_ASSERT_TRUE(threw, "the batch failed partway and rolled back");
    FATP_ASSERT_EQ(CountingRevertPolicy<uint16_t>::reverts, size_t(0),
                   "and the allocation policy was never reverted -- it issued nothing");
    FATP_ASSERT_EQ(sparse.recycled_count(), size_t(9), "the free set is exactly restored");
    FATP_ASSERT_EQ(sparse.free_interval_count(), size_t(2), "as the same two intervals");
    FATP_ASSERT_EQ(sparse.active_count(), size_t(1), "with only the claim still active");
    FATP_ASSERT_EQ(sparse.reserved_credit_count(), size_t(1), "and its credit alone");

    // Also confirm the preflight refuses domain exhaustion without rolling back.
    CountingRevertPolicy<uint16_t>::reverts = 0;
    auto doomed = sparse.generate_batch(12); // only 9 are free
    FATP_ASSERT_TRUE(!doomed.has_value() && doomed.error() == IdError::Overflow,
                     "an infeasible count is refused up front");
    FATP_ASSERT_EQ(CountingRevertPolicy<uint16_t>::reverts, size_t(0), "still no revert");

    // The counter is not inert: the ordinary path still reverts. This needs an
    // allocation policy that runs out while the CONFIGURED DOMAIN says otherwise,
    // because a domain that can be counted is now always caught by the preflight.
    using PlainGen = IdGenerator<uint64_t,
                                 CountingRevertPolicy<uint64_t>,
                                 ImmediateRecyclingPolicy<uint64_t>,
                                 id_generator::ExpectedErrorPolicy<uint64_t, IdError>,
                                 SingleThreadedPolicy>;

    PlainGen plain(0); // full-width domain: the preflight cannot bound it
    CountingRevertPolicy<uint64_t>::reverts = 0;
    CountingRevertPolicy<uint64_t>::issue_cap = 4; // but the allocator stops at four
    auto over = plain.generate_batch(10);
    CountingRevertPolicy<uint64_t>::issue_cap = 0;

    FATP_ASSERT_TRUE(!over.has_value() && over.error() == IdError::Overflow,
                     "an ordinary batch whose ALLOCATOR runs out mid-loop");
    FATP_ASSERT_TRUE(CountingRevertPolicy<uint64_t>::reverts > size_t(0),
                     "does revert -- so the zeros above are measurements, not a dead counter");
    FATP_ASSERT_EQ(plain.active_count(), size_t(0), "and the batch rolled fully back");
    return true;
}

#if FATP_USE_SHARED_MUTEX
// Constraint 8 requires claim() to take the generator's EXCLUSIVE lock, and the
// sparse policy has strictly more shared mutable state than any other -- an
// ordered map plus a credit list -- so an unsynchronised claim tears container
// internals rather than racing a counter. Deleting the lock passed the whole
// suite, because the one test naming ThreadSafeSparseIdGenerator ran entirely on
// the calling thread.
//
// UniqueRWLockPolicy rather than MutexSynchronizationPolicy: the latter's
// lock_shared() is an alias for its exclusive LockGuard over the same
// std::mutex, so swapping lock() for lock_shared() would be a no-op there. This
// is the only shipped policy whose shared lock is a real std::shared_lock, and
// it is the instantiation required test 23 asked for.
FATP_TEST_CASE(sparse_claims_are_serialized_under_contention)
{
    using RWSparse = IdGenerator<uint32_t,
                                 SequentialAllocationPolicy<uint32_t>,
                                 SparseRecyclingPolicy<uint32_t>,
                                 id_generator::ExpectedErrorPolicy<uint32_t, IdError>,
                                 UniqueRWLockPolicy>;

    static_assert(std::is_move_constructible_v<RWSparse>,
                  "UniqueRWLockPolicy holds its mutex in a unique_ptr and declares moves");
    static_assert(std::is_move_assignable_v<RWSparse>, "same");

    constexpr uint32_t kWriters = 4;
    constexpr uint32_t kIds = 40000;

    RWSparse gen(0, kIds - 1);

    std::atomic<size_t> claim_failures{0};
    std::atomic<size_t> read_failures{0};
    std::atomic<bool> done{false};

    std::vector<std::thread> threads;
    threads.reserve(kWriters + 2);

    // Interleaved stripes, so every claim contends on a node a neighbour is
    // splitting rather than on a disjoint region of the map.
    for (uint32_t t = 0; t < kWriters; ++t)
    {
        threads.emplace_back(
            [&gen, &claim_failures, t]()
            {
                for (uint32_t id = t; id < kIds; id += kWriters)
                {
                    if (!gen.claim(id).has_value())
                    {
                        claim_failures.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
    }

    // Readers hold the SHARED lock concurrently with the writers. They assert
    // only monotone bounds: sparse_invariants_hold composes three separately
    // locked reads and can legitimately observe C != A mid-flight on correct
    // code, so using it here would be a false failure, not a real one.
    for (int r = 0; r < 2; ++r)
    {
        threads.emplace_back(
            [&gen, &read_failures, &done]()
            {
                while (!done.load(std::memory_order_relaxed))
                {
                    const size_t active = gen.active_count();
                    const size_t free_ids = gen.recycled_count();
                    const size_t credits = gen.reserved_credit_count();
                    const size_t intervals = gen.free_interval_count();
                    if (active > kIds || free_ids > kIds || credits > kIds || intervals > kIds)
                    {
                        read_failures.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
    }

    for (uint32_t t = 0; t < kWriters; ++t)
    {
        threads[t].join();
    }
    done.store(true, std::memory_order_relaxed);
    for (size_t i = kWriters; i < threads.size(); ++i)
    {
        threads[i].join();
    }

    FATP_ASSERT_EQ(claim_failures.load(), size_t(0), "every claim in a disjoint set succeeded");
    FATP_ASSERT_EQ(read_failures.load(), size_t(0), "and no reader saw an impossible value");

    // Quiesced: now the exact composite state is meaningful.
    FATP_ASSERT_EQ(gen.active_count(), size_t(kIds), "every identifier is claimed");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(0), "nothing is free");
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(0), "which IS exhaustion");
    FATP_ASSERT_EQ(gen.reserved_credit_count(), size_t(kIds), "one credit per active identifier");

    auto over = gen.generate();
    FATP_ASSERT_TRUE(!over.has_value() && over.error() == IdError::Overflow, "and it refuses");

    for (uint32_t id = 0; id < kIds; ++id)
    {
        FATP_ASSERT_TRUE(gen.release(id).has_value(), "release everything");
    }
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1),
                   "and the domain merges back to exactly one interval");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(kIds), "holding every identifier");
    FATP_ASSERT_EQ(gen.reserved_credit_count(), size_t(0), "with every credit consumed");
    return true;
}
#endif // FATP_USE_SHARED_MUTEX

// A guard's destructor calls release(), which for this policy merges intervals.
// Neither the destructor nor move-assignment may terminate.
FATP_TEST_CASE(sparse_guards_release_through_the_interval_set)
{
    SparseIdGenerator<uint16_t> gen(0, 9);

    {
        auto guard = gen.scoped_id();
        FATP_ASSERT_TRUE(guard.has_value(), "a scoped identifier");
        FATP_ASSERT_EQ(gen.active_count(), size_t(1), "is active while held");
        FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1), "front trim, still one interval");
    }

    FATP_ASSERT_EQ(gen.active_count(), size_t(0), "and released on scope exit");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(10), "back into the free set");
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1), "merged, not appended");
    FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 10), "credits balance after a guard release");

    auto first = gen.scoped_id();
    auto second = gen.scoped_id();
    FATP_ASSERT_TRUE(first.has_value() && second.has_value(), "two guards");
    FATP_ASSERT_EQ(gen.active_count(), size_t(2), "both active");

    *first = std::move(*second); // drops first's identifier through release()
    FATP_ASSERT_EQ(gen.active_count(), size_t(1), "move-assignment released the dropped one");
    FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 10), "and balanced its credit");
    return true;
}

// Credits track the ACTIVE count, not the high-water mark: the reserved storage
// must come back down. std::forward_list has no capacity to retain, which is
// exactly why the credit stack is one.
FATP_TEST_CASE(sparse_credit_storage_returns_to_the_active_count)
{
    SparseIdGenerator<uint32_t> gen(0, 100000);

    std::vector<uint32_t> held;
    held.reserve(2000);
    for (int i = 0; i < 2000; ++i)
    {
        auto id = gen.generate();
        FATP_ASSERT_TRUE(id.has_value(), "activate a large set");
        held.push_back(*id);
    }
    FATP_ASSERT_EQ(gen.reserved_credit_count(), size_t(2000), "one credit per active identifier");

    for (uint32_t id : held)
    {
        FATP_ASSERT_TRUE(gen.release(id).has_value(), "release them all");
    }

    FATP_ASSERT_EQ(gen.reserved_credit_count(), size_t(0),
                   "the reserved footprint returns to zero, not to its high-water mark");
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1), "and the domain is whole again");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(100001), "every identifier back");
    return true;
}

// The pairing check and the constructor routing, asserted at compile time.
FATP_TEST_CASE(sparse_pairing_and_construction_are_compile_time_properties)
{
    // The predicate that drives the generator's static_assert. A compile-fail
    // harness would be needed to observe the assert itself firing; this asserts
    // the condition it fires on, which is the part that can be wrong.
    static_assert(!fat_p::detail::is_sequential_policy_v<RandomAllocationPolicy<uint64_t>>,
                  "random allocation is refused under a full-domain policy");
    static_assert(fat_p::detail::is_sequential_policy_v<SequentialAllocationPolicy<uint64_t>>, "allowed");
    static_assert(fat_p::detail::is_sequential_policy_v<BoundedSequentialAllocationPolicy<uint64_t>>,
                  "allowed");
    static_assert(fat_p::detail::is_sequential_policy_v<CountingRevertPolicy<uint64_t>>,
                  "and a custom policy may opt in, so the check is a property not a whitelist");

    static_assert(fat_p::detail::is_full_domain_policy_v<SparseRecyclingPolicy<uint64_t>>, "opted in");
    static_assert(!fat_p::detail::is_full_domain_policy_v<ImmediateRecyclingPolicy<uint64_t>>, "not");
    static_assert(fat_p::detail::has_domain_configure_v<SparseRecyclingPolicy<uint64_t>>, "configurable");
    static_assert(!fat_p::detail::has_domain_configure_v<MinRecyclingPolicy<uint64_t>>, "not");

    // Constructor routing, by policy role. Two arguments are available when
    // EITHER role opts in, and unavailable when neither does.
    static_assert(std::is_constructible_v<SparseIdGenerator<uint16_t>, uint16_t, uint16_t>,
                  "recycling-side opt-in reaches the two-argument constructor");
    static_assert(std::is_constructible_v<IdGenerator<uint16_t,
                                                      BoundedSequentialAllocationPolicy<uint16_t>>,
                                          uint16_t,
                                          uint16_t>,
                  "allocation-side opt-in does too");
    static_assert(std::is_constructible_v<IdGenerator<uint16_t,
                                                      BoundedSequentialAllocationPolicy<uint16_t>,
                                                      SparseRecyclingPolicy<uint16_t>>,
                                          uint16_t,
                                          uint16_t>,
                  "and so does both at once");
    static_assert(!std::is_constructible_v<SimpleIdGenerator<uint16_t>, uint16_t, uint16_t>,
                  "while a generator whose policies both decline does not offer it");

    // Both roles opted in: each must observe the SAME effective bound.
    IdGenerator<uint16_t,
                BoundedSequentialAllocationPolicy<uint16_t>,
                SparseRecyclingPolicy<uint16_t>,
                id_generator::ExpectedErrorPolicy<uint16_t, IdError>,
                SingleThreadedPolicy>
        both(10, 14);

    FATP_ASSERT_EQ(both.recycled_count(), size_t(5), "the recycling policy sees [10, 14]");
    auto outside = both.claim(15);
    FATP_ASSERT_TRUE(!outside.has_value() && outside.error() == IdError::InvalidClaim,
                     "and refuses above it");
    for (int i = 0; i < 5; ++i)
    {
        FATP_ASSERT_TRUE(both.generate().has_value(), "five issuable");
    }
    auto over = both.generate();
    FATP_ASSERT_TRUE(!over.has_value() && over.error() == IdError::Overflow,
                     "and the allocation policy saw the same bound");

    // Movability, measured rather than read off the declarations.
    static_assert(!std::is_move_constructible_v<SparseIdGenerator<uint64_t>>,
                  "the sparse alias is immovable, like every other shipped alias");
    using MovableSparse = IdGenerator<uint64_t,
                                      SequentialAllocationPolicy<uint64_t>,
                                      SparseRecyclingPolicy<uint64_t>,
                                      id_generator::ExpectedErrorPolicy<uint64_t, IdError>,
                                      MovableSingleThreadedPolicy>;
    static_assert(std::is_move_constructible_v<MovableSparse>,
                  "but an instantiation over a movable lock policy is movable -- which is the "
                  "sole reason reset() is conditionally noexcept");

    MovableSparse a(0, 9);
    FATP_ASSERT_TRUE(a.claim(4).has_value(), "claim before the move");
    MovableSparse b = std::move(a);
    FATP_ASSERT_TRUE(b.is_active(4), "the claim travelled");
    FATP_ASSERT_EQ(b.recycled_count(), size_t(9), "and so did the free set");
    return true;
}

// claim_at()'s fourth arm: the claimed identifier IS the whole interval, so the
// entry is ERASED rather than trimmed. Every other claim in this file front-trims,
// back-rekeys or splits. Regressing this arm to a front trim leaves an INVERTED
// entry (key k, mapped k+1), after which peek_lowest() hands generate() an
// identifier that is already active -- and every count still looks plausible.
FATP_TEST_CASE(sparse_claiming_a_singleton_interval_erases_it)
{
    // (a) A one-value free interval inside a larger domain.
    {
        SparseIdGenerator<uint16_t> gen(0, 9);
        for (uint16_t id : std::array<uint16_t, 3>{3, 4, 5})
        {
            FATP_ASSERT_TRUE(gen.claim(id).has_value(), "claim");
        }
        FATP_ASSERT_TRUE(gen.release(4).has_value(), "release the middle -> singleton [4,4]");
        FATP_ASSERT_EQ(gen.free_interval_count(), size_t(3), "[0,2] [4,4] [6,9]");

        FATP_ASSERT_TRUE(gen.claim(4).has_value(), "reclaim the singleton");
        FATP_ASSERT_EQ(gen.free_interval_count(), size_t(2),
                       "the entry is ERASED -- a front trim leaves an inverted [4,5]");
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(7), "3 + 4 free");
        FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 10), "invariants");

        // Drain the bottom interval, then prove issuance skips the active run.
        for (uint16_t expected : std::array<uint16_t, 3>{0, 1, 2})
        {
            auto got = gen.generate();
            FATP_ASSERT_TRUE(got.has_value() && *got == expected, "issues 0, 1, 2");
        }
        auto next = gen.generate();
        FATP_ASSERT_TRUE(next.has_value() && *next == uint16_t(6),
                         "then 6 -- never an identifier that is already active");
    }

    // (b) The single-value domain the note declares well-formed.
    {
        SparseIdGenerator<uint16_t> one(7, 7);
        FATP_ASSERT_EQ(one.recycled_count(), size_t(1), "one claimable identifier");
        FATP_ASSERT_EQ(one.free_interval_count(), size_t(1), "held as one interval");

        FATP_ASSERT_TRUE(one.claim(7).has_value(), "and it is claimable");
        FATP_ASSERT_EQ(one.free_interval_count(), size_t(0), "after which the map is empty");
        FATP_ASSERT_EQ(one.recycled_count(), size_t(0), "which IS exhaustion");
        FATP_ASSERT_EQ(one.reserved_credit_count(), size_t(1), "one credit for one active id");

        auto over = one.generate();
        FATP_ASSERT_TRUE(!over.has_value() && over.error() == IdError::Overflow, "exhausted");
        auto again = one.claim(7);
        FATP_ASSERT_TRUE(!again.has_value() && again.error() == IdError::AlreadyInUse, "duplicate");

        FATP_ASSERT_TRUE(one.release(7).has_value(), "release rebuilds the singleton");
        FATP_ASSERT_EQ(one.free_interval_count(), size_t(1), "[7,7] again");
        FATP_ASSERT_EQ(one.reserved_credit_count(), size_t(0), "credit consumed");

        FATP_ASSERT_TRUE(one.claim(7).has_value(), "claim again, then reset FROM EXHAUSTION");
        one.reset();
        FATP_ASSERT_EQ(one.free_interval_count(), size_t(1), "rebuilt from the credit");
        auto issued = one.generate();
        FATP_ASSERT_TRUE(issued.has_value() && *issued == uint16_t(7), "and 7 issues again");
    }
    return true;
}

// add_recycled()'s `id > mBase` guard is load-bearing in exactly ONE shape:
// base 0 AND a ceiling at the ID type's maximum. Only then does the wrapped
// `id - 1` land on a LIVE key -- the tail interval's -- so `left` and `right`
// resolve to the same entry, the merge-both arm self-assigns and then erases it,
// and the entire domain disappears. Every other sparse test releases inside a
// narrowed domain where the wrapped lookup harmlessly misses.
FATP_TEST_CASE(sparse_release_at_the_base_of_a_full_width_domain)
{
    // (a) The merge-back case: one interval, release the base.
    {
        SparseIdGenerator<uint8_t> gen;
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(256), "the full 0..255");

        auto first = gen.generate();
        FATP_ASSERT_TRUE(first.has_value() && *first == uint8_t(0), "issues 0");

        FATP_ASSERT_TRUE(gen.release(0).has_value(), "release the base");
        FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1),
                       "one interval -- an unguarded id-1 wraps onto the tail key and erases it");
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(256), "the whole domain is free again");
        FATP_ASSERT_EQ(gen.reserved_credit_count(), size_t(0), "credit consumed");

        auto again = gen.generate();
        FATP_ASSERT_TRUE(again.has_value() && *again == uint8_t(0), "and 0 issues again");
    }

    // (b) The extend-left case, with the domain fragmented so the wrapped
    //     lookup would rekey a different entry rather than merge with itself.
    {
        SparseIdGenerator<uint16_t> frag;
        for (int i = 0; i < 3; ++i)
        {
            FATP_ASSERT_TRUE(frag.generate().has_value(), "issue 0, 1, 2");
        }
        FATP_ASSERT_TRUE(frag.claim(100).has_value(), "and claim 100");
        FATP_ASSERT_EQ(frag.free_interval_count(), size_t(2), "[3,99] and [101,65535]");

        FATP_ASSERT_TRUE(frag.release(0).has_value(), "release the base");
        FATP_ASSERT_EQ(frag.free_interval_count(), size_t(3), "a singleton [0,0] appears");
        FATP_ASSERT_EQ(frag.recycled_count(), size_t(65533), "65536 - 3 active");
        FATP_ASSERT_TRUE(sparse_invariants_hold(frag, 65536), "invariants");
    }
    return true;
}

// release_batch() is a SEPARATE copy of release()'s erase-first ordering, and no
// test called it on a sparse generator at all. Invariant 3 constrains its
// mid-vector InvalidRelease return as much as its success return: hoisting
// add_recycled above the erase hands a never-active identifier to a policy that
// then calls take_credit() with no credit to take.
FATP_TEST_CASE(sparse_release_batch_keeps_the_existence_proof_first)
{
    // (a) A whole batch out and back.
    {
        SparseIdGenerator<uint16_t> gen(0, 9);
        auto batch = gen.generate_batch(4);
        FATP_ASSERT_TRUE(batch.has_value(), "four identifiers");
        FATP_ASSERT_EQ(gen.reserved_credit_count(), size_t(4), "one credit each");

        FATP_ASSERT_TRUE(gen.release_batch(*batch).has_value(), "released as a batch");
        FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1),
                       "merged back to ONE interval, not four fragments");
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(10), "the whole domain");
        FATP_ASSERT_EQ(gen.reserved_credit_count(), size_t(0), "every credit consumed");
    }

    // (b) A refused element mid-vector must insert nothing and consume nothing.
    {
        SparseIdGenerator<uint16_t> gen(0, 9);
        FATP_ASSERT_TRUE(gen.claim(2).has_value() && gen.claim(5).has_value(), "two claims");

        std::vector<uint16_t> ids{2, 4, 5}; // 4 was never active
        auto result = gen.release_batch(ids);
        FATP_ASSERT_TRUE(!result.has_value() && result.error() == IdError::InvalidRelease,
                         "the batch stops at the first non-active identifier");

        FATP_ASSERT_TRUE(!gen.is_active(2), "2 was released before the refusal");
        FATP_ASSERT_TRUE(gen.is_active(5), "5 was never reached");
        FATP_ASSERT_TRUE(!gen.is_active(4), "and 4 never became active");
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(9), "exactly one identifier came back");
        FATP_ASSERT_EQ(gen.reserved_credit_count(), size_t(1), "one credit left, for 5");
        FATP_ASSERT_TRUE(sparse_invariants_hold(gen, 10), "the refusal balanced");
    }
    return true;
}

// generate() constructs the caller's id_type BEFORE it mutates anything. Moving
// that construction last permanently strands an identifier: active, out of the
// free set, holding a credit, never handed to the caller -- and unrecoverable,
// because exhaustion is an empty map.
FATP_TEST_CASE(sparse_generate_constructs_the_id_type_before_mutating)
{
    ThrowingGenerator gen(0, 9);

    ThrowingId::constructed = 0;
    ThrowingId::throw_at = 0;

    bool threw = false;
    try
    {
        (void)gen.generate();
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    ThrowingId::throw_at = -1; // shared with the batch test

    FATP_ASSERT_TRUE(threw, "the constructor's exception reaches the caller");
    FATP_ASSERT_EQ(gen.active_count(), size_t(0), "nothing became active");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(10), "nothing left the free set");
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1), "the domain is untouched");
    FATP_ASSERT_EQ(gen.reserved_credit_count(), size_t(0), "and no credit was stranded");

    auto after = gen.generate();
    FATP_ASSERT_TRUE(after.has_value() && after->get() == uint16_t(0),
                     "the identifier it was about to strand is still issuable");
    return true;
}

// The reserved sentinel, across all three ways a domain can reach the top.
FATP_TEST_CASE(sparse_sentinel_normalization_covers_every_request)
{
    using Id = StrongId<uint8_t, struct SparseNormalizeTag>;

    // Default request: the whole domain, less the sentinel.
    {
        SparseIdGenerator<Id> gen;
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(255), "0..254, and not 255");

        size_t issued = 0;
        while (gen.generate().has_value())
        {
            ++issued;
        }
        FATP_ASSERT_EQ(issued, size_t(255), "exactly 255 identifiers");
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(0), "then nothing free");

        auto over = gen.generate();
        FATP_ASSERT_TRUE(!over.has_value() && over.error() == IdError::Overflow, "and Overflow");

        gen.reset();
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(255), "reset does not reintroduce 255");
        auto sentinel = gen.claim(Id::invalid());
        FATP_ASSERT_TRUE(!sentinel.has_value() && sentinel.error() == IdError::InvalidClaim,
                         "nor make it claimable");
    }

    // Explicit request of 255: normalized down, not honoured literally.
    {
        SparseIdGenerator<Id> gen(0, 255);
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(255), "an explicit 255 request normalizes");
        auto sentinel = gen.claim(Id::invalid());
        FATP_ASSERT_TRUE(!sentinel.has_value() && sentinel.error() == IdError::InvalidClaim,
                         "to a domain that excludes it");
    }

    // A plain uint8_t declares no sentinel, so 255 stays a perfectly good value.
    {
        SparseIdGenerator<uint8_t> gen;
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(256), "the full 0..255");
        FATP_ASSERT_TRUE(gen.claim(255).has_value(),
                         "255 is claimable when the ID type reserves nothing");
    }

    // A base ABOVE the normalized ceiling is a precondition violation, and the
    // release build is where it bites: this policy is the only guard against
    // issuing the sentinel, because the full-domain path has no late generate()
    // check to fall back on. An inverted interval used to hand 255 straight out.
    {
        SparseIdGenerator<Id> gen(255); // ceiling normalizes to 254, below the base
        FATP_ASSERT_EQ(gen.recycled_count(), size_t(0), "an empty domain, not an inverted one");
        FATP_ASSERT_EQ(gen.free_interval_count(), size_t(0), "which reads as exhausted");

        auto over = gen.generate();
        FATP_ASSERT_TRUE(!over.has_value() && over.error() == IdError::Overflow,
                         "so it refuses, exactly as the non-sparse path does");

        SimpleIdGenerator<Id> control(255);
        auto c = control.generate();
        FATP_ASSERT_TRUE(!c.has_value() && c.error() == IdError::Overflow,
                         "which is the control this now matches");
    }
    return true;
}

// The batch capacity preflight tests the CONFIGURED DOMAIN's width, not the ID
// type's. Gating it on sizeof(underlying_type) compiled it out for every 64-bit
// generator -- including a sparse one whose domain holds ten identifiers -- so
// an infeasible count reached result.reserve() and threw the exact exception the
// guard exists to prevent. That is the first consumer's shape: size_t-wide
// indices with a depth-derived ceiling.
FATP_TEST_CASE(sparse_batch_preflight_uses_the_configured_domain)
{
    SparseIdGenerator<uint64_t> wide(0, 9);

    // Catch rather than let it escape: without the fix this throws
    // std::length_error, and an uncaught throw aborts the runner instead of
    // reporting a named failure -- which is a kill, but an illegible one.
    bool threw = false;
    bool was_refused = false;
    try
    {
        auto absurd = wide.generate_batch(std::numeric_limits<size_t>::max());
        was_refused = !absurd.has_value() && absurd.error() == IdError::Overflow;
    }
    catch (...)
    {
        threw = true;
    }
    FATP_ASSERT_TRUE(!threw,
                     "an infeasible count is REFUSED, never thrown out of an Expected API");
    FATP_ASSERT_TRUE(was_refused, "and refused as Overflow");
    FATP_ASSERT_EQ(wide.recycled_count(), size_t(10), "with nothing consumed");
    FATP_ASSERT_EQ(wide.active_count(), size_t(0), "nor activated");

    auto over_by_one = wide.generate_batch(11);
    FATP_ASSERT_TRUE(!over_by_one.has_value() && over_by_one.error() == IdError::Overflow,
                     "the check is exact at the boundary");

    auto exact = wide.generate_batch(10);
    FATP_ASSERT_TRUE(exact.has_value() && exact->size() == size_t(10),
                     "and a count the domain can exactly satisfy still succeeds");

    // The same reasoning with the ceiling supplied by a bounded ALLOCATION
    // policy on a 64-bit type, where the guard was equally absent.
    IdGenerator<uint64_t, BoundedSequentialAllocationPolicy<uint64_t>> bounded(0, 4);
    auto refused = bounded.generate_batch(std::numeric_limits<size_t>::max());
    FATP_ASSERT_TRUE(!refused.has_value() && refused.error() == IdError::Overflow,
                     "bounded allocation is preflighted too");
    return true;
}

// A moved-from policy must be CONSISTENT, not merely empty. The implicit move
// gets this wrong invisibly: the containers move out and are left empty, but
// mCreditCount is a scalar and is COPIED -- so the source reports credits it
// does not hold, and configure_domain()'s reuse arm keys off exactly that count
// and calls take_credit() on an empty list.
FATP_TEST_CASE(sparse_moved_from_policy_reports_no_credits)
{
    using MovableSparse = IdGenerator<uint64_t,
                                      SequentialAllocationPolicy<uint64_t>,
                                      SparseRecyclingPolicy<uint64_t>,
                                      id_generator::ExpectedErrorPolicy<uint64_t, IdError>,
                                      MovableSingleThreadedPolicy>;

    MovableSparse a(0, 9);
    FATP_ASSERT_TRUE(a.claim(4).has_value(), "one claim, so one credit is held");
    FATP_ASSERT_EQ(a.reserved_credit_count(), size_t(1), "held before the move");

    MovableSparse b = std::move(a);

    FATP_ASSERT_EQ(b.active_count(), size_t(1), "the state travelled");
    FATP_ASSERT_EQ(b.reserved_credit_count(), size_t(1), "credits included");
    FATP_ASSERT_EQ(b.recycled_count(), size_t(9), "and the free set");

    // MovableSingleThreadedPolicy's locks are no-ops, so unlike the
    // UniqueRWLockPolicy case there is no null mutex making this UB.
    FATP_ASSERT_EQ(a.reserved_credit_count(), size_t(0),
                   "the SOURCE reports no credits, matching its emptied stack");
    FATP_ASSERT_EQ(a.active_count(), size_t(0), "and no actives");

    // The rebuild on a moved-from source must take the allocating arm -- which
    // is the entire stated basis for reset() being conditionally noexcept.
    a.reset();
    FATP_ASSERT_EQ(a.free_interval_count(), size_t(1), "reset rebuilt the domain");
    FATP_ASSERT_EQ(a.recycled_count(), size_t(10), "as the whole of [0, 9]");
    auto issued = a.generate();
    FATP_ASSERT_TRUE(issued.has_value() && *issued == uint64_t(0), "and it issues again");
    return true;
}

// The credit mechanism's PURPOSE, measured. Every other oracle in this file is a
// count, and no count can tell a node taken from the credit stack from one
// freshly allocated: both decrement the count and leave identical structure.
FATP_TEST_CASE(sparse_return_paths_allocate_nothing)
{
    SparseIdGenerator<uint16_t> gen(0, 63);

    // The counter is live: activation DOES allocate, one credit per identifier.
    std::vector<uint16_t> held;
    held.reserve(16);
    size_t activation_cost = 0;
    {
        allocprobe::Window w;
        for (int i = 0; i < 16; ++i)
        {
            auto id = gen.generate();
            if (id.has_value())
            {
                held.push_back(*id);
            }
        }
        activation_cost = w.count();
    }
    FATP_ASSERT_EQ(held.size(), size_t(16), "sixteen identifiers");
    FATP_ASSERT_TRUE(activation_cost > 0,
                     "activation allocates -- so a zero below is a measurement, not a dead probe");

    // Release: allocation-free, every transition.
    {
        allocprobe::Window w;
        for (uint16_t id : held)
        {
            (void)gen.release(id);
        }
        FATP_ASSERT_EQ(w.count(), size_t(0), "release() allocates NOTHING");
    }
    FATP_ASSERT_EQ(gen.free_interval_count(), size_t(1), "and canonicalizes");
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(64), "back to the whole domain");

    // release_batch: same promise, separate code path.
    {
        auto batch = gen.generate_batch(8);
        FATP_ASSERT_TRUE(batch.has_value(), "a batch to give back");
        allocprobe::Window w;
        (void)gen.release_batch(*batch);
        FATP_ASSERT_EQ(w.count(), size_t(0), "release_batch() allocates NOTHING");
    }

    // reset() on a fragmented domain: reuses a node from the map.
    {
        FATP_ASSERT_TRUE(gen.claim(10).has_value() && gen.claim(20).has_value(), "fragment it");
        allocprobe::Window w;
        gen.reset();
        FATP_ASSERT_EQ(w.count(), size_t(0), "reset() reuses an owned node");
    }
    FATP_ASSERT_EQ(gen.recycled_count(), size_t(64), "rebuilt");

    // reset() from EXHAUSTION: no map node left, so it must spend a credit.
    {
        SparseIdGenerator<uint8_t> small(0, 3);
        for (int i = 0; i < 4; ++i)
        {
            (void)small.generate();
        }
        FATP_ASSERT_EQ(small.free_interval_count(), size_t(0), "no node to reuse");
        FATP_ASSERT_EQ(small.reserved_credit_count(), size_t(4), "but credits are held");

        allocprobe::Window w;
        small.reset();
        FATP_ASSERT_EQ(w.count(), size_t(0),
                       "the exhausted rebuild spends a credit rather than allocating");
        FATP_ASSERT_EQ(small.recycled_count(), size_t(4), "and the domain is whole");
    }
    return true;
}

// reserve_claim_credits()'s unwind: an interior claim reserves TWO nodes, and if
// the second fails the first must be released. FlakySparsePolicy cannot see this
// -- it throws before delegating -- so only a failure injected at the allocator
// reaches the partial state.
FATP_TEST_CASE(sparse_split_reservation_unwinds_its_first_credit)
{
    // Drive the policy directly: the whole reservation surface is public.
    SparseRecyclingPolicy<uint16_t> policy;
    policy.configure_domain(0, 9);

    // How many allocations does one interior reservation perform? Do not
    // hard-code it -- it is two credits here, but measuring keeps the test
    // honest if the node source ever changes.
    size_t cost = 0;
    {
        allocprobe::Window w;
        const std::size_t reserved = policy.reserve_claim_credits(5);
        cost = w.count();
        FATP_ASSERT_EQ(reserved, size_t(2), "an interior claim reserves two nodes");
    }
    FATP_ASSERT_TRUE(cost >= 2, "each credit is a real allocation");
    for (std::size_t i = 0; i < 2; ++i)
    {
        policy.discard_credit();
    }
    FATP_ASSERT_EQ(policy.credit_count(), size_t(0), "baseline restored");

    // Now fail each allocation of that reservation in turn. Every one must
    // leave the policy exactly as it was found.
    for (size_t fail_index = 0; fail_index < cost; ++fail_index)
    {
        bool threw = false;
        try
        {
            allocprobe::Window w(static_cast<long long>(fail_index));
            (void)policy.reserve_claim_credits(5);
        }
        catch (const std::bad_alloc&)
        {
            threw = true;
        }

        FATP_ASSERT_TRUE(threw, "the injected failure propagates");
        FATP_ASSERT_EQ(policy.credit_count(), size_t(0),
                       "no credit survives the unwind -- a bare rethrow leaks the first one");
        FATP_ASSERT_EQ(policy.interval_count(), size_t(1), "and no interval was disturbed");
        FATP_ASSERT_EQ(policy.recycled_count(), size_t(10), "nor any identifier consumed");
    }

    // And the reservation still works afterwards.
    FATP_ASSERT_EQ(policy.reserve_claim_credits(5), size_t(2), "usable after every failure");
    return true;
}

} // namespace fat_p::testing::idgenerator

namespace fat_p::testing
{


inline void run_benchmarks()
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

    // Coverage added after the component review
    FATP_RUN_TEST_NS(runner, idgenerator, already_in_use_is_reachable);
    FATP_RUN_TEST_NS(runner, idgenerator, collision_retry_loop_is_load_bearing);
    FATP_RUN_TEST_NS(runner, idgenerator, exhausted_latch_holds_until_reset);
    FATP_RUN_TEST_NS(runner, idgenerator, batch_rollback_restores_the_pool_exactly);
    FATP_RUN_TEST_NS(runner, idgenerator, revert_does_not_over_rewind_at_saturation);
    FATP_RUN_TEST_NS(runner, idgenerator, reset_invalidates_outstanding_guards);
    FATP_RUN_TEST_NS(runner, idgenerator, guard_move_assignment_releases_the_dropped_id);
    FATP_RUN_TEST_NS(runner, idgenerator, strong_id_never_issues_the_invalid_sentinel);
    FATP_RUN_TEST_NS(runner, idgenerator, random_retries_the_sentinel_instead_of_reporting_overflow);
    FATP_RUN_TEST_NS(runner, idgenerator, bounded_revert_does_not_over_rewind_at_the_bound);
    FATP_RUN_TEST_NS(runner, idgenerator, bounded_generator_honors_its_upper_bound);
    FATP_RUN_TEST_NS(runner, idgenerator, random_honors_base_id_as_a_lower_bound);
    FATP_RUN_TEST_NS(runner, idgenerator, batch_refuses_an_impossible_count);
    FATP_RUN_TEST_NS(runner, idgenerator, strong_id_batch_guard_and_queries);
    FATP_RUN_TEST_NS(runner, idgenerator, movability_matches_the_concurrency_policy);

    // Sparse ID claiming
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_claims_the_domain_endpoints);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_claim_does_not_consume_lower_identifiers);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_claim_order_does_not_change_the_result);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_duplicate_claim_is_refused_without_side_effects);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_out_of_domain_claim_is_invalid_claim);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_domain_excludes_the_reserved_sentinel);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_release_then_reclaim);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_release_transitions_canonicalize);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_generation_returns_the_lowest_free_identifier);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_batch_excludes_claims_and_rolls_back_exactly);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_exhaustion_is_the_configured_ceiling);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_recycled_count_is_exact_then_saturates);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_reset_rebuilds_the_configured_domain);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_reset_from_exhaustion_reuses_a_credit);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_contract_holds_under_mutex_synchronization);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_policy_does_not_reach_the_existing_aliases);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_credit_failure_leaves_the_free_set_untouched);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_batch_unwinds_a_throwing_id_constructor);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_claim_credit_failure_changes_nothing);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_claiming_a_singleton_interval_erases_it);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_release_at_the_base_of_a_full_width_domain);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_release_batch_keeps_the_existence_proof_first);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_generate_constructs_the_id_type_before_mutating);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_batch_preflight_uses_the_configured_domain);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_moved_from_policy_reports_no_credits);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_return_paths_allocate_nothing);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_split_reservation_unwinds_its_first_credit);
#if FATP_USE_SHARED_MUTEX
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_claims_are_serialized_under_contention);
#endif
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_claim_cost_is_independent_of_the_gap);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_batch_rollback_does_not_revert_the_allocation_policy);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_guards_release_through_the_interval_set);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_credit_storage_returns_to_the_active_count);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_pairing_and_construction_are_compile_time_properties);
    FATP_RUN_TEST_NS(runner, idgenerator, sparse_sentinel_normalization_covers_every_request);


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
