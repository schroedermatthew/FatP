#include <iostream>
#include <thread>
#include <vector>

#include "StringPool.h"
#include "test_StringPool.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{


bool test_string_pool_basic_interning() {
    StringPool pool;
    
    const char* s1 = pool.intern("hello");
    const char* s2 = pool.intern("hello");
    
    SIMPLE_ASSERT(s1 == s2, "Same string should return same pointer");
    SIMPLE_ASSERT(std::string_view(s1) == "hello", "Content should match");
    return true;
}

bool test_string_pool_different_strings() {
    StringPool pool;
    
    const char* s1 = pool.intern("hello");
    const char* s2 = pool.intern("world");
    
    SIMPLE_ASSERT(s1 != s2, "Different strings should have different pointers");
    return true;
}

bool test_string_pool_memory_savings() {
    StringPool pool;
    
    std::vector<const char*> pointers;
    for (int i = 0; i < 100; ++i) {
        pointers.push_back(pool.intern("duplicate"));
    }
    
    // All pointers should be identical
    for (size_t i = 1; i < pointers.size(); ++i) {
        SIMPLE_ASSERT(pointers[i] == pointers[0], "All duplicates should point to same string");
    }
    
    auto stats = pool.stats();
    SIMPLE_ASSERT(stats.unique_strings == 1, "Should have only 1 unique string");
    SIMPLE_ASSERT(stats.hit_rate >= 0.99, "Hit rate should be very high (99/100 = 0.99)");
    
    return true;
}

bool test_string_pool_thread_safety() {
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

void benchmark_string_pool() {
    std::cout << "\n" << colors::cyan() << "StringPool Benchmarks:" << colors::reset() << "\n\n";
    
    // Benchmark 1: First intern (miss)
    {
        StringPool pool;
        double time = measure_perf([&pool, i=0]() mutable {
            pool.intern("unique_string_" + std::to_string(i++));
        }, 1000, 10);
        std::cout << "First intern (miss): " << format_time(time) << "\n";
    }
    
    // Benchmark 2: Subsequent intern (hit)
    {
        StringPool pool;
        pool.intern("cached");
        
        double time = measure_perf([&pool]() {
            pool.intern("cached");
        }, 10000, 100);
        std::cout << "Subsequent intern (hit): " << format_time(time) << "\n";
    }
    
    // Benchmark 3: Memory savings
    {
        StringPool pool;
        constexpr int NUM_DUPLICATES = 10000;
        const std::string str = "This is a moderately long string that gets duplicated many times";
        
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

bool test_StringPool() {

    PRINT_HEADER(STRING POOL)

    TestRunner runner;

    RUN_TEST(runner, string_pool_basic_interning);
    RUN_TEST(runner, string_pool_different_strings);
    RUN_TEST(runner, string_pool_memory_savings);
    RUN_TEST(runner, string_pool_thread_safety);

    benchmark_string_pool();

    return 0 == runner.print_summary();

}

} // namespace cpp_utilities::testing
