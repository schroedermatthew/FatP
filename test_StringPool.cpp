#include <iostream>
#include <thread>
#include <vector>
#include <map>

#include "StringPool.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_StringPool.h"
#endif

namespace fat_p::testing
{

// anonymous
namespace
{

bool test_basic_interning() {
    StringPool pool;
    
    const char* s1 = pool.intern("hello");
    const char* s2 = pool.intern("hello");
    
    SIMPLE_ASSERT(s1 == s2, "Same string should return same pointer");
    SIMPLE_ASSERT(std::string_view(s1) == "hello", "Content should match");
    return true;
}

bool test_different_strings() {
    StringPool pool;
    
    const char* s1 = pool.intern("hello");
    const char* s2 = pool.intern("world");
    
    SIMPLE_ASSERT(s1 != s2, "Different strings should have different pointers");
    return true;
}

bool test_memory_savings() {
    StringPool pool;
    
    std::vector<const char*> pointers;
    for (int i = 0; i < 100; ++i) {
        pointers.push_back(pool.intern("duplicate"));
    }
    
    for (size_t i = 1; i < pointers.size(); ++i) {
        SIMPLE_ASSERT(pointers[i] == pointers[0], 
                     "All duplicates should point to same string");
    }
    
    auto stats = pool.stats();
    SIMPLE_ASSERT(stats.unique_strings == 1, "Should have only 1 unique string");
    SIMPLE_ASSERT(stats.hit_rate >= 0.99, "Hit rate should be very high (99/100 = 0.99)");
    
    return true;
}

bool test_thread_safety() {
    StringPool pool;
    std::atomic<int> matches{0};
    
    auto worker = [&pool, &matches]() {
        for (int i = 0; i < 1000; ++i) {
            const char* s = pool.intern("thread_safe_test");
            if (std::string_view(s) == "thread_safe_test") {
                matches.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    SIMPLE_ASSERT(matches.load() == 4000, "All strings should be interned correctly");
    SIMPLE_ASSERT(pool.size() == 1, "Should have only 1 unique string");
    
    return true;
}

bool test_empty_string() {
    StringPool pool;
    
    const char* s1 = pool.intern("");
    const char* s2 = pool.intern(std::string_view(""));
    const char* s3 = pool.intern(std::string(""));
    
    SIMPLE_ASSERT(s1 == s2, "Empty strings via string_view should be deduplicated");
    SIMPLE_ASSERT(s1 == s3, "Empty strings via std::string should be deduplicated");
    SIMPLE_ASSERT(std::string_view(s1) == "", "Empty string content should be empty");
    
    auto stats = pool.stats();
    SIMPLE_ASSERT(stats.unique_strings == 1, "Should have only 1 unique empty string");
    
    return true;
}

bool test_nullptr_handling() {
    StringPool pool;
    
    const char* s1 = pool.intern(nullptr);
    const char* s2 = pool.intern("");
    
    SIMPLE_ASSERT(s1 != nullptr, "nullptr should return valid pointer");
    SIMPLE_ASSERT(s1 == s2, "nullptr should be treated as empty string");
    SIMPLE_ASSERT(std::string_view(s1) == "", "nullptr result should be empty");
    
    return true;
}

bool test_clear_behavior() {
    StringPool pool;
    
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
    SIMPLE_ASSERT(stats_after.bytes_allocated == 0, 
                 "bytes_allocated should be 0 after clear");
    SIMPLE_ASSERT(stats_after.memory_saved == 0, 
                 "memory_saved should be 0 after clear");
    SIMPLE_ASSERT(pool.size() == 0, "Pool size should be 0 after clear");
    SIMPLE_ASSERT(pool.empty(), "Pool should be empty after clear");
    
    return true;
}

bool test_reset_stats() {
    StringPool pool;
    
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
    SIMPLE_ASSERT(stats_after.bytes_allocated > 0, 
                 "bytes_allocated should reflect current pool usage");
    
    return true;
}

bool test_string_handle_comparison() {
    StringPool pool;
    
    StringHandle h1(pool.intern("aaa"));
    StringHandle h2(pool.intern("bbb"));
    StringHandle h3(pool.intern("aaa"));
    StringHandle h_null;
    
    SIMPLE_ASSERT(h1 == h3, "Equal handles should compare equal");
    SIMPLE_ASSERT(h1 != h2, "Different handles should compare not equal");
    SIMPLE_ASSERT(h1 < h2, "Handles should support ordering (aaa < bbb)");
    SIMPLE_ASSERT(!(h2 < h1), "Ordering should be consistent");
    SIMPLE_ASSERT(h_null < h1, "Null handle should be less than any valid handle");
    SIMPLE_ASSERT(!h_null, "Null handle should be false");
    SIMPLE_ASSERT(h1.operator bool(), "Non-null handle should be true");
    
    return true;
}

bool test_string_handle_operations() {
    StringPool pool;
    
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

bool test_string_handle_in_container() {
    StringPool pool;
    
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
    
    return true;
}

bool test_long_strings() {
    StringPool pool;
    
    std::string long_str(10000, 'x');
    const char* s1 = pool.intern(long_str);
    const char* s2 = pool.intern(long_str);
    
    SIMPLE_ASSERT(s1 == s2, "Long strings should be deduplicated");
    SIMPLE_ASSERT(std::string_view(s1).size() == 10000, 
                 "Long string length should be preserved");
    
    auto stats = pool.stats();
    SIMPLE_ASSERT(stats.bytes_allocated == 10001, 
                 "Should allocate 10000 chars + null terminator");
    SIMPLE_ASSERT(stats.memory_saved == 10001, 
                 "Should save 10001 bytes on second intern");
    
    return true;
}

bool test_unicode_strings() {
    StringPool pool;
    
    const char* s1 = pool.intern("Hello World");
    const char* s2 = pool.intern("Hello World");
    const char* s3 = pool.intern("Bonjour");
    const char* s4 = pool.intern("Nihao");
    
    SIMPLE_ASSERT(s1 == s2, "Unicode strings should be deduplicated");
    SIMPLE_ASSERT(s1 != s3, "Different Unicode strings should differ");
    SIMPLE_ASSERT(s1 != s4, "Different Unicode strings should differ");
    
    SIMPLE_ASSERT(std::string_view(s1) == "Hello World", 
                 "Unicode content should be preserved");
    SIMPLE_ASSERT(std::string_view(s3) == "Bonjour", 
                 "Unicode content should be preserved");
    SIMPLE_ASSERT(std::string_view(s4) == "Nihao", 
                 "Unicode content should be preserved");
    
    return true;
}

bool test_memory_statistics_accuracy() {
    StringPool pool;
    
    const std::string test_str = "test123";
    
    pool.intern(test_str);
    auto stats1 = pool.stats();
    
    SIMPLE_ASSERT(stats1.bytes_allocated == 8, "Should allocate 8 bytes (7 + null)");
    SIMPLE_ASSERT(stats1.unique_strings == 1, "Should have 1 unique string");
    SIMPLE_ASSERT(stats1.total_interns == 1, "Should have 1 total intern");
    SIMPLE_ASSERT(stats1.memory_saved == 0, "No savings on first intern");
    
    pool.intern(test_str);
    auto stats2 = pool.stats();
    
    SIMPLE_ASSERT(stats2.bytes_allocated == 8, "Bytes allocated shouldn't change");
    SIMPLE_ASSERT(stats2.unique_strings == 1, "Should still have 1 unique string");
    SIMPLE_ASSERT(stats2.total_interns == 2, "Should have 2 total interns");
    SIMPLE_ASSERT(stats2.memory_saved == 8, "Should save 8 bytes");
    
    return true;
}

bool test_contains_and_find() {
    StringPool pool;
    
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

bool test_intern_overloads() {
    StringPool pool;
    
    const char* s1 = pool.intern("test");
    const char* s2 = pool.intern(std::string("test"));
    const char* s3 = pool.intern(std::string_view("test"));
    
    SIMPLE_ASSERT(s1 == s2, "C string and std::string should intern to same pointer");
    SIMPLE_ASSERT(s1 == s3, "C string and string_view should intern to same pointer");
    
    auto stats = pool.stats();
    SIMPLE_ASSERT(stats.unique_strings == 1, "All overloads should deduplicate");
    
    return true;
}

bool test_size_and_empty() {
    StringPool pool;
    
    SIMPLE_ASSERT(pool.empty(), "New pool should be empty");
    SIMPLE_ASSERT(pool.size() == 0, "New pool size should be 0");
    
    pool.intern("first");
    SIMPLE_ASSERT(!pool.empty(), "Pool should not be empty after intern");
    SIMPLE_ASSERT(pool.size() == 1, "Pool size should be 1");
    
    pool.intern("second");
    SIMPLE_ASSERT(pool.size() == 2, "Pool size should be 2");
    
    pool.intern("first");
    SIMPLE_ASSERT(pool.size() == 2, "Pool size should still be 2 after duplicate");
    
    return true;
}

bool test_hit_rate_calculation() {
    StringPool pool;
    
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

bool test_whitespace_strings() {
    StringPool pool;
    
    const char* s1 = pool.intern("   ");
    const char* s2 = pool.intern("\t\t");
    const char* s3 = pool.intern("\n\n");
    const char* s4 = pool.intern("   ");
    
    SIMPLE_ASSERT(s1 != s2, "Different whitespace should not be deduplicated");
    SIMPLE_ASSERT(s1 != s3, "Different whitespace should not be deduplicated");
    SIMPLE_ASSERT(s1 == s4, "Same whitespace should be deduplicated");
    
    return true;
}

bool test_special_characters() {
    StringPool pool;
    
    const char* s1 = pool.intern("test@#$%");
    const char* s2 = pool.intern("test@#$%");
    const char* s3 = pool.intern("\"quoted\"");
    
    SIMPLE_ASSERT(s1 == s2, "Strings with special chars should be deduplicated");
    SIMPLE_ASSERT(std::string_view(s1) == "test@#$%", "Content should be preserved");
    SIMPLE_ASSERT(std::string_view(s3) == "\"quoted\"", "Quoted content preserved");
    
    return true;
}

bool test_case_sensitivity() {
    StringPool pool;
    
    const char* s1 = pool.intern("Test");
    const char* s2 = pool.intern("test");
    const char* s3 = pool.intern("TEST");
    const char* s4 = pool.intern("Test");
    
    SIMPLE_ASSERT(s1 != s2, "Different case strings should not be deduplicated");
    SIMPLE_ASSERT(s1 != s3, "Different case strings should not be deduplicated");
    SIMPLE_ASSERT(s1 == s4, "Same case strings should be deduplicated");
    
    auto stats = pool.stats();
    SIMPLE_ASSERT(stats.unique_strings == 3, "Should have 3 unique strings");
    
    return true;
}

void benchmark_string_pool() {
    std::cout << "\n" << colors::cyan() << "StringPool Benchmarks:" 
              << colors::reset() << "\n\n";
    
    {
        StringPool pool;
        double time = measure_perf([&pool, i=0]() mutable {
            pool.intern("unique_string_" + std::to_string(i++));
        }, 1000, 10);
        std::cout << "First intern (miss): " << format_time(time) << "\n";
    }
    
    {
        StringPool pool;
        pool.intern("cached");
        
        double time = measure_perf([&pool]() {
            pool.intern("cached");
        }, 10000, 100);
        std::cout << "Subsequent intern (hit): " << format_time(time) << "\n";
    }
    
    {
        StringPool pool;
        constexpr int NUM_DUPLICATES = 10000;
        const std::string str = 
            "This is a moderately long string that gets duplicated many times";
        
        for (int i = 0; i < NUM_DUPLICATES; ++i) {
            pool.intern(str);
        }
        
        auto stats = pool.stats();
        size_t saved = stats.memory_saved;
        std::cout << "Memory saved (" << NUM_DUPLICATES << " duplicates): " 
                  << colors::green() << saved << " bytes" 
                  << colors::reset() << "\n";
    }
}

} // anonymous namespace

bool test_StringPool() {

    PRINT_HEADER(STRING POOL)

    TestRunner runner;

    RUN_TEST(runner, basic_interning);
    RUN_TEST(runner, different_strings);
    RUN_TEST(runner, memory_savings);
    RUN_TEST(runner, thread_safety);
    RUN_TEST(runner, empty_string);
    RUN_TEST(runner, nullptr_handling);
    RUN_TEST(runner, clear_behavior);
    RUN_TEST(runner, reset_stats);
    RUN_TEST(runner, string_handle_comparison);
    RUN_TEST(runner, string_handle_operations);
    RUN_TEST(runner, string_handle_in_container);
    RUN_TEST(runner, long_strings);
    RUN_TEST(runner, unicode_strings);
    RUN_TEST(runner, memory_statistics_accuracy);
    RUN_TEST(runner, contains_and_find);
    RUN_TEST(runner, intern_overloads);
    RUN_TEST(runner, size_and_empty);
    RUN_TEST(runner, hit_rate_calculation);
    RUN_TEST(runner, whitespace_strings);
    RUN_TEST(runner, special_characters);
    RUN_TEST(runner, case_sensitivity);

    benchmark_string_pool();

    return 0 == runner.print_summary();

}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_StringPool() ? 0 : 1;
}
#endif
