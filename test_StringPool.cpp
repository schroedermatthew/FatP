#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <thread>
#include <unordered_map>
#include <vector>

#include "StringPool.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_StringPool.h"
#endif

namespace fat_p::testing::stringpool
{

// ============================================================================
// Basic Interning Tests
// ============================================================================

TEST_CASE(basic_interning)
{
    StringPool<> pool;

    const char* s1 = pool.intern("hello");
    const char* s2 = pool.intern("hello");

    ASSERT_EQ(s1, s2, "Same string should return same pointer");
    ASSERT_EQ(std::string_view(s1), std::string_view("hello"), "Content should match");
    return true;
}

TEST_CASE(different_strings)
{
    StringPool<> pool;

    const char* s1 = pool.intern("hello");
    const char* s2 = pool.intern("world");

    ASSERT_NE(s1, s2, "Different strings should have different pointers");
    return true;
}

TEST_CASE(memory_savings)
{
    StringPool<> pool;

    std::vector<const char*> pointers;
    for (int i = 0; i < 100; ++i)
    {
        pointers.push_back(pool.intern("duplicate"));
    }

    for (size_t i = 1; i < pointers.size(); ++i)
    {
        ASSERT_EQ(pointers[i], pointers[0],
                  "All duplicates should point to same string");
    }

    auto stats = pool.stats();
    ASSERT_EQ(stats.unique_strings, size_t(1), "Should have only 1 unique string");
    SIMPLE_ASSERT(stats.hit_rate >= 0.99, "Hit rate should be very high (99/100 = 0.99)");

    return true;
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_CASE(empty_string)
{
    StringPool<> pool;

    const char* s1 = pool.intern("");
    const char* s2 = pool.intern(std::string_view(""));
    const char* s3 = pool.intern(std::string(""));

    ASSERT_EQ(s1, s2, "Empty strings via string_view should be deduplicated");
    ASSERT_EQ(s1, s3, "Empty strings via std::string should be deduplicated");
    ASSERT_EQ(std::string_view(s1), std::string_view(""), "Empty string content should be empty");

    auto stats = pool.stats();
    ASSERT_EQ(stats.unique_strings, size_t(1), "Should have only 1 unique empty string");

    return true;
}

TEST_CASE(nullptr_handling)
{
    StringPool<> pool;

    const char* s1 = pool.intern(nullptr);
    const char* s2 = pool.intern("");

    ASSERT_NE(s1, nullptr, "nullptr should return valid pointer");
    ASSERT_EQ(s1, s2, "nullptr should be treated as empty string");
    ASSERT_EQ(std::string_view(s1), std::string_view(""), "nullptr result should be empty");

    return true;
}

TEST_CASE(long_strings)
{
    StringPool<> pool;

    std::string long_str(10000, 'x');
    const char* s1 = pool.intern(long_str);
    const char* s2 = pool.intern(long_str);

    ASSERT_EQ(s1, s2, "Long strings should be deduplicated");
    ASSERT_EQ(std::string_view(s1).size(), size_t(10000),
              "Long string length should be preserved");

    auto stats = pool.stats();
    ASSERT_EQ(stats.content_bytes, size_t(10001),
              "Should track 10000 chars + null terminator");
    ASSERT_EQ(stats.memory_saved, size_t(10001),
              "Should save 10001 bytes on second intern");

    return true;
}

TEST_CASE(reset_stats_accuracy)
{
    StringPool<> pool;

    pool.intern("test1");  // +6 bytes (5 + null)
    pool.intern("test2");  // +6 bytes
    pool.intern("test1");  // hit, saved 6

    auto stats_before = pool.stats();
    ASSERT_EQ(stats_before.content_bytes, size_t(12), "Pre-reset content_bytes");
    ASSERT_EQ(stats_before.total_interns, size_t(3), "Pre-reset total_interns");
    ASSERT_EQ(stats_before.memory_saved, size_t(6), "Pre-reset memory_saved");

    pool.reset_stats();

    auto stats_after = pool.stats();
    ASSERT_EQ(stats_after.content_bytes, size_t(12), "Post-reset content_bytes unchanged");
    ASSERT_EQ(stats_after.total_interns, size_t(2), "total_interns reset to size");
    ASSERT_EQ(stats_after.memory_saved, size_t(0), "memory_saved reset to 0");
    ASSERT_EQ(stats_after.unique_strings, size_t(2), "unique_strings from container");

    return true;
}

TEST_CASE(utf8_strings)
{
    StringPool<> pool;

    const char* s1 = pool.intern(u8"Hello World");
    const char* s2 = pool.intern(u8"Hello World");
    const char* s3 = pool.intern(u8"Bonjour le monde");
    const char* s4 = pool.intern(u8"Hallo Welt");

    ASSERT_EQ(s1, s2, "UTF-8 strings should be deduplicated");
    ASSERT_NE(s1, s3, "Different UTF-8 strings should differ");
    ASSERT_NE(s1, s4, "Different UTF-8 strings should differ");

    ASSERT_EQ(std::string_view(s1), std::string_view(u8"Hello World"),
              "UTF-8 content should be preserved");
    ASSERT_EQ(std::string_view(s3), std::string_view(u8"Bonjour le monde"),
              "UTF-8 content should be preserved");

    return true;
}

TEST_CASE(whitespace_strings)
{
    StringPool<> pool;

    const char* s1 = pool.intern("   ");
    const char* s2 = pool.intern("\t\t");
    const char* s3 = pool.intern("\n\n");
    const char* s4 = pool.intern("   ");

    ASSERT_NE(s1, s2, "Different whitespace should not be deduplicated");
    ASSERT_NE(s1, s3, "Different whitespace should not be deduplicated");
    ASSERT_EQ(s1, s4, "Same whitespace should be deduplicated");

    return true;
}

TEST_CASE(special_characters)
{
    StringPool<> pool;

    const char* s1 = pool.intern("test@#$%");
    const char* s2 = pool.intern("test@#$%");
    const char* s3 = pool.intern("\"quoted\"");

    ASSERT_EQ(s1, s2, "Strings with special chars should be deduplicated");
    ASSERT_EQ(std::string_view(s1), std::string_view("test@#$%"), "Content should be preserved");
    ASSERT_EQ(std::string_view(s3), std::string_view("\"quoted\""), "Quoted content preserved");

    return true;
}

TEST_CASE(case_sensitivity)
{
    StringPool<> pool;

    const char* s1 = pool.intern("Test");
    const char* s2 = pool.intern("test");
    const char* s3 = pool.intern("TEST");
    const char* s4 = pool.intern("Test");

    ASSERT_NE(s1, s2, "Different case strings should not be deduplicated");
    ASSERT_NE(s1, s3, "Different case strings should not be deduplicated");
    ASSERT_EQ(s1, s4, "Same case strings should be deduplicated");

    auto stats = pool.stats();
    ASSERT_EQ(stats.unique_strings, size_t(3), "Should have 3 unique strings");

    return true;
}

// ============================================================================
// Pool Management Tests
// ============================================================================

TEST_CASE(clear_behavior)
{
    StringPool<> pool;

    pool.intern("test1");
    pool.intern("test2");
    pool.intern("test1");

    auto stats_before = pool.stats();
    SIMPLE_ASSERT(stats_before.unique_strings == 2, "Should have 2 unique strings");
    SIMPLE_ASSERT(stats_before.total_interns == 3, "Should have 3 total interns");

    pool.clear();

    auto stats_after = pool.stats();
    SIMPLE_ASSERT(stats_after.unique_strings == 0,
                  "unique_strings should be 0 after clear");
    SIMPLE_ASSERT(stats_after.total_interns == 0,
                  "total_interns should be 0 after clear");
    SIMPLE_ASSERT(stats_after.content_bytes == 0,
                  "content_bytes should be 0 after clear");
    SIMPLE_ASSERT(stats_after.memory_saved == 0,
                  "memory_saved should be 0 after clear");
    SIMPLE_ASSERT(pool.size() == 0, "Pool size should be 0 after clear");
    SIMPLE_ASSERT(pool.empty(), "Pool should be empty after clear");

    return true;
}

TEST_CASE(reset_stats)
{
    StringPool<> pool;

    pool.intern("test1");
    pool.intern("test2");
    pool.intern("test1");

    auto stats_before = pool.stats();
    SIMPLE_ASSERT(stats_before.total_interns == 3, "Should have 3 total interns");
    SIMPLE_ASSERT(stats_before.memory_saved > 0, "Should have memory savings");

    pool.reset_stats();

    auto stats_after = pool.stats();
    SIMPLE_ASSERT(stats_after.unique_strings == 2,
                  "unique_strings should equal pool size after reset");
    SIMPLE_ASSERT(stats_after.total_interns == 2,
                  "total_interns should equal pool size after reset");
    SIMPLE_ASSERT(stats_after.memory_saved == 0,
                  "memory_saved should be 0 after reset");
    SIMPLE_ASSERT(stats_after.content_bytes > 0,
                  "content_bytes should reflect current pool content");

    return true;
}

TEST_CASE(contains_and_find)
{
    StringPool<> pool;

    pool.intern("exists");

    SIMPLE_ASSERT(pool.contains("exists"), "Should contain interned string");
    SIMPLE_ASSERT(!pool.contains("not_exists"), "Should not contain non-interned string");

    const char* found = pool.find("exists");
    const char* not_found = pool.find("not_exists");

    SIMPLE_ASSERT(found != nullptr, "find() should return non-null for existing string");
    SIMPLE_ASSERT(not_found == nullptr, "find() should return null for non-existing string");
    SIMPLE_ASSERT(std::string_view(found) == "exists",
                  "Found string should have correct content");

    return true;
}

TEST_CASE(intern_overloads)
{
    StringPool<> pool;

    const char* s1 = pool.intern("test");
    const char* s2 = pool.intern(std::string("test"));
    const char* s3 = pool.intern(std::string_view("test"));

    SIMPLE_ASSERT(s1 == s2, "C string and std::string should intern to same pointer");
    SIMPLE_ASSERT(s1 == s3, "C string and string_view should intern to same pointer");

    auto stats = pool.stats();
    SIMPLE_ASSERT(stats.unique_strings == 1, "All overloads should deduplicate");

    return true;
}

TEST_CASE(size_and_empty)
{
    StringPool<> pool;

    SIMPLE_ASSERT(pool.empty(), "New pool should be empty");
    ASSERT_EQ(pool.size(), size_t(0), "New pool size should be 0");

    pool.intern("first");
    SIMPLE_ASSERT(!pool.empty(), "Pool should not be empty after intern");
    ASSERT_EQ(pool.size(), size_t(1), "Pool size should be 1");

    pool.intern("second");
    ASSERT_EQ(pool.size(), size_t(2), "Pool size should be 2");

    pool.intern("first");
    ASSERT_EQ(pool.size(), size_t(2), "Pool size should still be 2 after duplicate");

    return true;
}

TEST_CASE(hit_rate_calculation)
{
    StringPool<> pool;

    pool.intern("a");
    pool.intern("b");
    pool.intern("c");
    pool.intern("a");
    pool.intern("b");
    pool.intern("a");

    auto stats = pool.stats();

    SIMPLE_ASSERT(stats.unique_strings == 3, "Should have 3 unique strings");
    SIMPLE_ASSERT(stats.total_interns == 6, "Should have 6 total interns");

    double expected_hit_rate = 3.0 / 6.0;
    double tolerance = 0.001;
    SIMPLE_ASSERT(std::abs(stats.hit_rate - expected_hit_rate) < tolerance,
                  "Hit rate should be 0.5 (3 hits out of 6 interns)");

    return true;
}

TEST_CASE(statistics_accuracy)
{
    StringPool<> pool;

    const std::string test_str = "test123";

    pool.intern(test_str);
    auto stats1 = pool.stats();

    ASSERT_EQ(stats1.content_bytes, size_t(8), "Should track 8 bytes (7 + null)");
    ASSERT_EQ(stats1.unique_strings, size_t(1), "Should have 1 unique string");
    ASSERT_EQ(stats1.total_interns, size_t(1), "Should have 1 total intern");
    ASSERT_EQ(stats1.memory_saved, size_t(0), "No savings on first intern");

    pool.intern(test_str);
    auto stats2 = pool.stats();

    ASSERT_EQ(stats2.content_bytes, size_t(8), "Content bytes shouldn't change");
    ASSERT_EQ(stats2.unique_strings, size_t(1), "Should still have 1 unique string");
    ASSERT_EQ(stats2.total_interns, size_t(2), "Should have 2 total interns");
    ASSERT_EQ(stats2.memory_saved, size_t(8), "Should save 8 bytes");

    return true;
}

// ============================================================================
// StringHandle Tests
// ============================================================================

TEST_CASE(handle_comparison)
{
    StringPool<> pool;

    StringHandle h1(pool.intern("aaa"));
    StringHandle h2(pool.intern("bbb"));
    StringHandle h3(pool.intern("aaa"));
    StringHandle h_null;

    // Equality is pointer-based
    SIMPLE_ASSERT(h1 == h3, "Equal handles should compare equal (same pointer)");
    SIMPLE_ASSERT(h1 != h2, "Different handles should compare not equal");

    // Ordering is also pointer-based (NOT alphabetical)
    // We can only test consistency, not specific order (addresses are arbitrary)
    SIMPLE_ASSERT(!(h1 < h3) && !(h3 < h1), "Equal handles should not be less than each other");
    SIMPLE_ASSERT((h1 < h2) != (h2 < h1), "Different handles must have strict ordering");

    // Null handling: std::less<const char*> defines nullptr as minimal
    SIMPLE_ASSERT(h_null < h1 || h1 < h_null, "Null and non-null must have ordering");
    SIMPLE_ASSERT(!h_null, "Null handle should be false");
    SIMPLE_ASSERT(static_cast<bool>(h1), "Non-null handle should be true");

    return true;
}

TEST_CASE(handle_operations)
{
    StringPool<> pool;

    const char* ptr = pool.intern("test");
    StringHandle handle(ptr);

    SIMPLE_ASSERT(handle.get() == ptr, "get() should return original pointer");
    SIMPLE_ASSERT(std::string_view(handle.c_str()) == "test",
                  "c_str() should return string content");

    const char* implicit = handle;
    SIMPLE_ASSERT(implicit == ptr, "Implicit conversion to const char* should work");

    std::string_view sv = handle;
    SIMPLE_ASSERT(sv == "test", "Implicit conversion to string_view should work");

    StringHandle empty;
    SIMPLE_ASSERT(std::string_view(empty.c_str()) == "",
                  "Empty handle c_str() should return empty string");

    return true;
}

TEST_CASE(handle_in_map)
{
    StringPool<> pool;

    // Note: std::map ordering is by pointer address, NOT alphabetical
    std::map<StringHandle, int> map;

    StringHandle h1(pool.intern("alpha"));
    StringHandle h2(pool.intern("beta"));
    StringHandle h3(pool.intern("gamma"));

    map[h1] = 1;
    map[h2] = 2;
    map[h3] = 3;

    SIMPLE_ASSERT(map.size() == 3, "Map should have 3 entries");
    SIMPLE_ASSERT(map[h1] == 1, "Should retrieve correct value for h1");
    SIMPLE_ASSERT(map[h2] == 2, "Should retrieve correct value for h2");
    SIMPLE_ASSERT(map[h3] == 3, "Should retrieve correct value for h3");

    // Verify lookup works with equivalent handles
    StringHandle h1_copy(pool.intern("alpha"));
    SIMPLE_ASSERT(h1 == h1_copy, "Same string should yield same handle");
    SIMPLE_ASSERT(map[h1_copy] == 1, "Lookup with equivalent handle should work");

    return true;
}

TEST_CASE(handle_in_unordered_map)
{
    StringPool<> pool;

    std::unordered_map<StringHandle, int> map;

    StringHandle h1(pool.intern("alpha"));
    StringHandle h2(pool.intern("beta"));
    StringHandle h3(pool.intern("gamma"));

    map[h1] = 1;
    map[h2] = 2;
    map[h3] = 3;

    SIMPLE_ASSERT(map.size() == 3, "Unordered map should have 3 entries");
    SIMPLE_ASSERT(map[h1] == 1, "Should retrieve correct value for h1");
    SIMPLE_ASSERT(map[h2] == 2, "Should retrieve correct value for h2");
    SIMPLE_ASSERT(map[h3] == 3, "Should retrieve correct value for h3");

    return true;
}

// ============================================================================
// Thread Safety Tests (using SharedMutexPolicy)
// ============================================================================

TEST_CASE(thread_safety_shared_mutex)
{
    StringPool<SharedMutexPolicy> pool;
    std::atomic<int> matches{0};

    auto worker = [&pool, &matches]() {
        for (int i = 0; i < 1000; ++i)
        {
            const char* s = pool.intern("thread_safe_test");
            if (std::string_view(s) == "thread_safe_test")
            {
                matches.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back(worker);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    SIMPLE_ASSERT(matches.load() == 4000, "All strings should be interned correctly");
    SIMPLE_ASSERT(pool.size() == 1, "Should have only 1 unique string");

    return true;
}

TEST_CASE(thread_safety_mutex)
{
    StringPool<MutexSynchronizationPolicy> pool;
    std::atomic<int> success_count{0};

    auto worker = [&pool, &success_count](int thread_id) {
        for (int i = 0; i < 500; ++i)
        {
            std::string unique_str = "thread_" + std::to_string(thread_id) + "_iter_" +
                                     std::to_string(i);
            const char* s = pool.intern(unique_str);
            if (s != nullptr && std::string_view(s) == unique_str)
            {
                success_count.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    SIMPLE_ASSERT(success_count.load() == 2000, "All interns should succeed");
    SIMPLE_ASSERT(pool.size() == 2000, "Should have 2000 unique strings");

    return true;
}

TEST_CASE(concurrent_read_write)
{
    StringPool<SharedMutexPolicy> pool;

    // Pre-populate with some strings
    for (int i = 0; i < 100; ++i)
    {
        pool.intern("preload_" + std::to_string(i));
    }

    std::atomic<int> read_success{0};
    std::atomic<int> write_success{0};

    auto reader = [&pool, &read_success]() {
        for (int i = 0; i < 500; ++i)
        {
            if (pool.contains("preload_50"))
            {
                read_success.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    auto writer = [&pool, &write_success]() {
        for (int i = 0; i < 500; ++i)
        {
            const char* s = pool.intern("new_string_" + std::to_string(i));
            if (s != nullptr)
            {
                write_success.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    threads.emplace_back(reader);
    threads.emplace_back(reader);
    threads.emplace_back(writer);
    threads.emplace_back(writer);

    for (auto& t : threads)
    {
        t.join();
    }

    ASSERT_EQ(read_success.load(), 1000, "All reads should succeed");
    ASSERT_EQ(write_success.load(), 1000, "All writes should succeed");

    return true;
}

TEST_CASE(concurrent_clear)
{
    StringPool<SharedMutexPolicy> pool;
    std::atomic<bool> done{false};
    std::atomic<int> intern_count{0};
    std::atomic<int> clear_count{0};
    bool timed_out = false;

    auto interner = [&pool, &done, &intern_count]() {
        while (!done.load(std::memory_order_acquire))
        {
            pool.intern("test_string");
            intern_count.fetch_add(1, std::memory_order_relaxed);
        }
    };

    auto clearer = [&pool, &done, &clear_count]() {
        while (!done.load(std::memory_order_acquire))
        {
            pool.clear();
            clear_count.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    };

    std::thread t1(interner);
    std::thread t2(interner);
    std::thread t3(clearer);

    // Wait until we have meaningful activity (with timeout to prevent CI hangs)
    auto start = std::chrono::steady_clock::now();
    while (intern_count.load(std::memory_order_relaxed) < 100 ||
           clear_count.load(std::memory_order_relaxed) < 3)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5))
        {
            timed_out = true;
            break;
        }
    }

    // Signal threads to stop
    done.store(true, std::memory_order_release);

    t1.join();
    t2.join();
    t3.join();

    // Fail explicitly on timeout
    SIMPLE_ASSERT(!timed_out, "Timeout waiting for concurrent activity - possible deadlock");

    // Test passes if no crashes occurred during concurrent operations
    SIMPLE_ASSERT(intern_count.load() >= 100, "Sufficient interns should have occurred");
    SIMPLE_ASSERT(clear_count.load() >= 3, "Sufficient clears should have occurred");

    // Final clear to verify pool is still functional
    pool.clear();
    ASSERT_EQ(pool.size(), size_t(0), "Pool should be empty after final clear");

    return true;
}

TEST_CASE(concurrent_reads)
{
    StringPool<SharedMutexPolicy> pool;

    // Pre-populate
    for (int i = 0; i < 100; ++i)
    {
        pool.intern("key_" + std::to_string(i));
    }

    std::atomic<size_t> successes{0};

    auto reader = [&pool, &successes]() {
        for (int i = 0; i < 1000; ++i)
        {
            if (pool.contains("key_50"))
            {
                successes.fetch_add(1, std::memory_order_relaxed);
            }
            if (pool.find("key_50") != nullptr)
            {
                successes.fetch_add(1, std::memory_order_relaxed);
            }
            if (pool.size() == 100)
            {
                successes.fetch_add(1, std::memory_order_relaxed);
            }
            if (!pool.empty())
            {
                successes.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i)
    {
        threads.emplace_back(reader);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // 8 threads × 1000 iterations × 4 checks = 32000 successes
    ASSERT_EQ(successes.load(), size_t(32000), "All concurrent reads should succeed");

    return true;
}

// ============================================================================
// Policy-Specific Tests
// ============================================================================

TEST_CASE(single_threaded_policy)
{
    StringPool<SingleThreadedPolicy> pool;

    const char* s1 = pool.intern("single_thread");
    const char* s2 = pool.intern("single_thread");

    SIMPLE_ASSERT(s1 == s2, "SingleThreadedPolicy should work correctly");
    SIMPLE_ASSERT(pool.size() == 1, "Should have 1 unique string");

    return true;
}

TEST_CASE(shared_mutex_policy)
{
    StringPool<SharedMutexPolicy> pool;

    const char* s1 = pool.intern("shared_mutex");
    const char* s2 = pool.intern("shared_mutex");

    SIMPLE_ASSERT(s1 == s2, "SharedMutexPolicy should work correctly");
    SIMPLE_ASSERT(pool.size() == 1, "Should have 1 unique string");

    return true;
}

TEST_CASE(mutex_policy)
{
    StringPool<MutexSynchronizationPolicy> pool;

    const char* s1 = pool.intern("mutex_sync");
    const char* s2 = pool.intern("mutex_sync");

    SIMPLE_ASSERT(s1 == s2, "MutexSynchronizationPolicy should work correctly");
    SIMPLE_ASSERT(pool.size() == 1, "Should have 1 unique string");

    return true;
}

TEST_CASE(reserve_capacity)
{
    StringPool<> pool;

    // Reserve space for expected strings
    pool.reserve(1000);

    // Bulk insert without rehashing
    for (int i = 0; i < 500; ++i)
    {
        pool.intern("string_" + std::to_string(i));
    }

    ASSERT_EQ(pool.size(), size_t(500), "Should have 500 unique strings");

    // Verify pool still functions correctly after reserve
    const char* s1 = pool.intern("test");
    const char* s2 = pool.intern("test");
    ASSERT_EQ(s1, s2, "Deduplication should work after reserve");

    return true;
}

TEST_CASE(concurrent_stats_consistency)
{
    StringPool<SharedMutexPolicy> pool;
    std::atomic<bool> done{false};
    std::atomic<int> violations{0};

    auto writer = [&pool, &done]() {
        int i = 0;
        while (!done.load(std::memory_order_acquire))
        {
            pool.intern("concurrent_" + std::to_string(i++ % 100));
        }
    };

    auto stater = [&pool, &done, &violations]() {
        while (!done.load(std::memory_order_acquire))
        {
            auto s = pool.stats();
            // Check invariants
            if (s.total_interns < s.unique_strings)
            {
                violations.fetch_add(1, std::memory_order_relaxed);
            }
            if (s.hit_rate < 0.0 || s.hit_rate > 1.0)
            {
                violations.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    threads.emplace_back(writer);
    threads.emplace_back(writer);
    threads.emplace_back(stater);
    threads.emplace_back(stater);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    done.store(true, std::memory_order_release);

    for (auto& t : threads)
    {
        t.join();
    }

    ASSERT_EQ(violations.load(), 0, "No stats invariant violations should occur");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_string_pool()
{
    std::cout << "\n" << colors::cyan() << "StringPool Benchmarks:"
              << colors::reset() << "\n\n";

    // Benchmark: First intern (cache miss)
    {
        StringPool<> pool;
        int i = 0;
        double time = measure_perf([&pool, &i]() {
            auto ptr = pool.intern("unique_string_" + std::to_string(i++));
            DoNotOptimize(ptr);
        }, 1000, 10);
        std::cout << "First intern (miss): " << format_time(time) << "\n";
    }

    // Benchmark: Subsequent intern (cache hit)
    {
        StringPool<> pool;
        pool.intern("cached");

        double time = measure_perf([&pool]() {
            auto ptr = pool.intern("cached");
            DoNotOptimize(ptr);
        }, 10000, 100);
        std::cout << "Subsequent intern (hit): " << format_time(time) << "\n";
    }

    // Benchmark: Memory savings
    {
        StringPool<> pool;
        constexpr int NUM_DUPLICATES = 10000;
        const std::string str =
            "This is a moderately long string that gets duplicated many times";

        for (int i = 0; i < NUM_DUPLICATES; ++i)
        {
            pool.intern(str);
        }

        auto stats = pool.stats();
        size_t saved = stats.memory_saved;
        std::cout << "Memory saved (" << NUM_DUPLICATES << " duplicates): "
                  << colors::green() << saved << " bytes"
                  << colors::reset() << "\n";
    }

    // Benchmark: SharedMutexPolicy overhead (single-threaded)
    {
        StringPool<SharedMutexPolicy> pool;
        pool.intern("cached");

        double time = measure_perf([&pool]() {
            auto ptr = pool.intern("cached");
            DoNotOptimize(ptr);
        }, 10000, 100);
        std::cout << "SharedMutexPolicy intern (hit, single-thread): " << format_time(time) << "\n";
    }

    // Benchmark: Multi-threaded SharedMutexPolicy
    {
        StringPool<SharedMutexPolicy> pool;
        pool.intern("cached_multi");

        constexpr int THREADS = 4;
        constexpr int OPS_PER_THREAD = 2500;

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int t = 0; t < THREADS; ++t)
        {
            threads.emplace_back([&pool]() {
                for (int i = 0; i < OPS_PER_THREAD; ++i)
                {
                    auto ptr = pool.intern("cached_multi");
                    DoNotOptimize(ptr);
                }
            });
        }

        for (auto& t : threads)
        {
            t.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        double total_ns = std::chrono::duration<double, std::nano>(end - start).count();
        double per_op_ns = total_ns / (THREADS * OPS_PER_THREAD);

        std::cout << "SharedMutexPolicy intern (hit, " << THREADS << " threads): "
                  << std::fixed << std::setprecision(1) << per_op_ns << " ns/op\n";
    }

    // Benchmark: Multi-threaded SharedMutexPolicy misses (unique strings)
    {
        StringPool<SharedMutexPolicy> pool;

        constexpr int THREADS = 4;
        constexpr int OPS_PER_THREAD = 2500;

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int t = 0; t < THREADS; ++t)
        {
            threads.emplace_back([&pool, t]() {
                for (int i = 0; i < OPS_PER_THREAD; ++i)
                {
                    auto ptr = pool.intern("unique_" + std::to_string(t * OPS_PER_THREAD + i));
                    DoNotOptimize(ptr);
                }
            });
        }

        for (auto& t : threads)
        {
            t.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        double total_ns = std::chrono::duration<double, std::nano>(end - start).count();
        double per_op_ns = total_ns / (THREADS * OPS_PER_THREAD);

        std::cout << "SharedMutexPolicy intern (miss, " << THREADS << " threads): "
                  << std::fixed << std::setprecision(1) << per_op_ns << " ns/op\n";
    }

    // Benchmark: contains() lookup
    {
        StringPool<> pool;
        pool.intern("lookup_target");

        double time = measure_perf([&pool]() {
            bool found = pool.contains("lookup_target");
            DoNotOptimize(found);
        }, 10000, 100);
        std::cout << "contains() lookup: " << format_time(time) << "\n";
    }

    // Benchmark: find() lookup
    {
        StringPool<> pool;
        pool.intern("find_target");

        double time = measure_perf([&pool]() {
            auto ptr = pool.find("find_target");
            DoNotOptimize(ptr);
        }, 10000, 100);
        std::cout << "find() lookup: " << format_time(time) << "\n";
    }

    // Benchmark: reserve() impact on bulk insertion of unique strings
    {
        constexpr int BULK_SIZE = 500;
        constexpr int TRIALS = 5;

        double total_unreserved = 0.0;
        double total_reserved = 0.0;

        for (int trial = 0; trial < TRIALS; ++trial)
        {
            int base = trial * BULK_SIZE;

            // Without reserve
            {
                StringPool<> pool;
                int i = 0;
                double time = measure_perf([&pool, &i, base]() {
                    auto ptr = pool.intern("bulk_" + std::to_string(base + i++));
                    DoNotOptimize(ptr);
                }, BULK_SIZE, 0);
                total_unreserved += time;
            }

            // With reserve
            {
                StringPool<> pool;
                pool.reserve(BULK_SIZE);
                int j = 0;
                double time = measure_perf([&pool, &j, base]() {
                    auto ptr = pool.intern("bulk_" + std::to_string(base + j++));
                    DoNotOptimize(ptr);
                }, BULK_SIZE, 0);
                total_reserved += time;
            }
        }

        std::cout << "Bulk insert unique (no reserve): "
                  << format_time(total_unreserved / TRIALS) << "\n";
        std::cout << "Bulk insert unique (with reserve): "
                  << format_time(total_reserved / TRIALS) << "\n";
    }
}

} // namespace fat_p::testing::stringpool

namespace fat_p::testing
{

bool test_StringPool()
{
    PRINT_HEADER(STRING POOL)

    TestRunner runner;

    // Basic interning tests
    RUN_TEST_NS(runner, stringpool, basic_interning);
    RUN_TEST_NS(runner, stringpool, different_strings);
    RUN_TEST_NS(runner, stringpool, memory_savings);

    // Edge case tests
    RUN_TEST_NS(runner, stringpool, empty_string);
    RUN_TEST_NS(runner, stringpool, nullptr_handling);
    RUN_TEST_NS(runner, stringpool, long_strings);
    RUN_TEST_NS(runner, stringpool, utf8_strings);
    RUN_TEST_NS(runner, stringpool, whitespace_strings);
    RUN_TEST_NS(runner, stringpool, special_characters);
    RUN_TEST_NS(runner, stringpool, case_sensitivity);

    // Pool management tests
    RUN_TEST_NS(runner, stringpool, clear_behavior);
    RUN_TEST_NS(runner, stringpool, reset_stats);
    RUN_TEST_NS(runner, stringpool, reset_stats_accuracy);
    RUN_TEST_NS(runner, stringpool, contains_and_find);
    RUN_TEST_NS(runner, stringpool, intern_overloads);
    RUN_TEST_NS(runner, stringpool, size_and_empty);
    RUN_TEST_NS(runner, stringpool, hit_rate_calculation);
    RUN_TEST_NS(runner, stringpool, statistics_accuracy);

    // StringHandle tests
    RUN_TEST_NS(runner, stringpool, handle_comparison);
    RUN_TEST_NS(runner, stringpool, handle_operations);
    RUN_TEST_NS(runner, stringpool, handle_in_map);
    RUN_TEST_NS(runner, stringpool, handle_in_unordered_map);

    // Thread safety tests
    RUN_TEST_NS(runner, stringpool, thread_safety_shared_mutex);
    RUN_TEST_NS(runner, stringpool, thread_safety_mutex);
    RUN_TEST_NS(runner, stringpool, concurrent_read_write);
    RUN_TEST_NS(runner, stringpool, concurrent_clear);
    RUN_TEST_NS(runner, stringpool, concurrent_reads);
    RUN_TEST_NS(runner, stringpool, concurrent_stats_consistency);

    // Policy-specific tests
    RUN_TEST_NS(runner, stringpool, single_threaded_policy);
    RUN_TEST_NS(runner, stringpool, shared_mutex_policy);
    RUN_TEST_NS(runner, stringpool, mutex_policy);
    RUN_TEST_NS(runner, stringpool, reserve_capacity);

    stringpool::benchmark_string_pool();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_StringPool() ? 0 : 1;
}
#endif
