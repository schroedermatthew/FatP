/**
 * @file test_SmallVector_Enhanced.cpp
 * @brief Enhanced test suite for SmallVector with exception safety, concurrency, and serialization tests
 * @version 2.1
 * @date 2025
 */

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <memory>
#include <thread>
#include <future>
#include <sstream>

#include "SmallVector.h"
#include "test_SmallVector.h"
#include "FatPTest.h"
#include "ConcurrencyPolicies.h"
#include "BinarySerializer.h"

namespace fat_p::testing
{

constexpr size_t INLINE_CAPACITY = 8;

class ThrowingCopyType {
public:
    static inline int copy_throw_after = -1;
    static inline int copy_count = 0;
    static inline int construct_count = 0;
    static inline int destruct_count = 0;
    
    int value;
    
    explicit ThrowingCopyType(int v = 0) : value(v) {
        ++construct_count;
    }
    
    ThrowingCopyType(const ThrowingCopyType& other) : value(other.value) {
        ++copy_count;
        if (copy_throw_after >= 0 && copy_count >= copy_throw_after) {
            throw std::runtime_error("Intentional copy throw");
        }
    }
    
    ThrowingCopyType(ThrowingCopyType&& other) noexcept : value(other.value) {
    }
    
    ~ThrowingCopyType() {
        ++destruct_count;
    }
    
    static void reset() {
        copy_throw_after = -1;
        copy_count = 0;
        construct_count = 0;
        destruct_count = 0;
    }
};

class ThrowingMoveType {
public:
    static inline int move_throw_after = -1;
    static inline int move_count = 0;
    static inline int construct_count = 0;
    static inline int destruct_count = 0;
    
    int value;
    
    explicit ThrowingMoveType(int v = 0) : value(v) {
        ++construct_count;
    }
    
    ThrowingMoveType(const ThrowingMoveType& other) : value(other.value) {
    }
    
    ThrowingMoveType(ThrowingMoveType&& other) : value(other.value) {
        ++move_count;
        if (move_throw_after >= 0 && move_count >= move_throw_after) {
            throw std::runtime_error("Intentional move throw");
        }
    }
    
    ~ThrowingMoveType() {
        ++destruct_count;
    }
    
    static void reset() {
        move_throw_after = -1;
        move_count = 0;
        construct_count = 0;
        destruct_count = 0;
    }
};

bool test_SmallVector_ExceptionSafety_InsertMultiple() {
    SUBTEST("Exception safety: insert multiple copies") {
        ThrowingCopyType::reset();
        ThrowingCopyType::copy_throw_after = 3;
        
        SmallVector<ThrowingCopyType, 4> vec;
        vec.push_back(ThrowingCopyType{1});
        vec.push_back(ThrowingCopyType{2});
        
        size_t original_size = vec.size();
        bool threw = false;
        
        try {
            vec.insert(vec.begin() + 1, 5, ThrowingCopyType{42});
        } catch (const std::runtime_error&) {
            threw = true;
        }
        
        ASSERT_TRUE(threw, "Should have thrown");
        ASSERT_EQ(vec.size(), original_size, "Size should be unchanged (strong guarantee)");
        ASSERT_EQ(vec[0].value, 1, "Original element 0 should be preserved");
        ASSERT_EQ(vec[1].value, 2, "Original element 1 should be preserved");
        
        ASSERT_EQ(ThrowingCopyType::construct_count, 
                  ThrowingCopyType::destruct_count,
                  "No resource leaks");
    }
    END_SUBTEST;
    
    return get_subtest_tracker().all_passed();
}

bool test_SmallVector_ExceptionSafety_InsertRange() {
    SUBTEST("Exception safety: insert range") {
        ThrowingCopyType::reset();
        ThrowingCopyType::copy_throw_after = 2;
        
        SmallVector<ThrowingCopyType, 4> vec;
        vec.push_back(ThrowingCopyType{1});
        vec.push_back(ThrowingCopyType{2});
        
        std::vector<ThrowingCopyType> to_insert = {
            ThrowingCopyType{10}, 
            ThrowingCopyType{20}, 
            ThrowingCopyType{30}
        };
        
        size_t original_size = vec.size();
        bool threw = false;
        
        try {
            vec.insert(vec.begin() + 1, to_insert.begin(), to_insert.end());
        } catch (const std::runtime_error&) {
            threw = true;
        }
        
        ASSERT_TRUE(threw, "Should have thrown");
        ASSERT_EQ(vec.size(), original_size, "Size should be unchanged");
        
        ASSERT_EQ(ThrowingCopyType::construct_count, 
                  ThrowingCopyType::destruct_count,
                  "No resource leaks");
    }
    END_SUBTEST;
    
    return get_subtest_tracker().all_passed();
}

bool test_SmallVector_ExceptionSafety_AssignCount() {
    SUBTEST("Exception safety: assign count") {
        ThrowingCopyType::reset();
        ThrowingCopyType::copy_throw_after = 3;
        
        SmallVector<ThrowingCopyType, 4> vec;
        vec.push_back(ThrowingCopyType{1});
        vec.push_back(ThrowingCopyType{2});
        
        bool threw = false;
        
        try {
            vec.assign(5, ThrowingCopyType{42});
        } catch (const std::runtime_error&) {
            threw = true;
        }
        
        ASSERT_TRUE(threw, "Should have thrown");
        ASSERT_EQ(vec.size(), 0, "Size should be 0 (cleared before construction)");
        
        ASSERT_EQ(ThrowingCopyType::construct_count, 
                  ThrowingCopyType::destruct_count,
                  "No resource leaks");
    }
    END_SUBTEST;
    
    return get_subtest_tracker().all_passed();
}

bool test_SmallVector_ExceptionSafety_AssignRange() {
    SUBTEST("Exception safety: assign range") {
        ThrowingCopyType::reset();
        ThrowingCopyType::copy_throw_after = 2;
        
        SmallVector<ThrowingCopyType, 4> vec;
        vec.push_back(ThrowingCopyType{1});
        
        std::vector<ThrowingCopyType> to_assign = {
            ThrowingCopyType{10}, 
            ThrowingCopyType{20}, 
            ThrowingCopyType{30}
        };
        
        bool threw = false;
        
        try {
            vec.assign(to_assign.begin(), to_assign.end());
        } catch (const std::runtime_error&) {
            threw = false;
        }
        
        ASSERT_TRUE(threw, "Should have thrown");
        ASSERT_EQ(vec.size(), 0, "Size should be 0");
        
        ASSERT_EQ(ThrowingCopyType::construct_count, 
                  ThrowingCopyType::destruct_count,
                  "No resource leaks");
    }
    END_SUBTEST;
    
    return get_subtest_tracker().all_passed();
}

bool test_SmallVector_ExceptionSafety_Resize() {
    SUBTEST("Exception safety: resize with value") {
        ThrowingCopyType::reset();
        ThrowingCopyType::copy_throw_after = 3;
        
        SmallVector<ThrowingCopyType, 4> vec;
        vec.push_back(ThrowingCopyType{1});
        
        size_t original_size = vec.size();
        bool threw = false;
        
        try {
            vec.resize(6, ThrowingCopyType{42});
        } catch (const std::runtime_error&) {
            threw = true;
        }
        
        ASSERT_TRUE(threw, "Should have thrown");
        ASSERT_EQ(vec.size(), original_size, "Size should be unchanged");
        
        ASSERT_EQ(ThrowingCopyType::construct_count, 
                  ThrowingCopyType::destruct_count,
                  "No resource leaks");
    }
    END_SUBTEST;
    
    return get_subtest_tracker().all_passed();
}

bool test_SmallVector_ExceptionSafety_Emplace() {
    SUBTEST("Exception safety: emplace") {
        ThrowingCopyType::reset();
        ThrowingCopyType::copy_throw_after = 1;
        
        SmallVector<ThrowingCopyType, 4> vec;
        vec.push_back(ThrowingCopyType{1});
        vec.push_back(ThrowingCopyType{2});
        vec.push_back(ThrowingCopyType{3});
        
        size_t original_size = vec.size();
        bool threw = false;
        
        ThrowingCopyType temp{42};
        try {
            vec.emplace(vec.begin() + 1, temp);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        
        ASSERT_TRUE(threw, "Should have thrown");
        ASSERT_EQ(vec.size(), original_size, "Size should be unchanged");
        ASSERT_EQ(vec[0].value, 1, "Element 0 preserved");
        ASSERT_EQ(vec[1].value, 2, "Element 1 preserved");
        ASSERT_EQ(vec[2].value, 3, "Element 2 preserved");
        
        ASSERT_EQ(ThrowingCopyType::construct_count, 
                  ThrowingCopyType::destruct_count,
                  "No resource leaks");
    }
    END_SUBTEST;
    
    return get_subtest_tracker().all_passed();
}

bool test_SmallVector_ThreadSafety_ConcurrentReads() {
    SUBTEST("Thread safety: concurrent reads") {
        SmallVector<int, 8, 2, std::allocator<int>, ReadWriteLock> vec;
        
        for (int i = 0; i < 100; ++i) {
            vec.push_back(i);
        }
        
        std::atomic<int> success_count{0};
        std::vector<std::thread> threads;
        
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&vec, &success_count]() {
                for (int i = 0; i < 1000; ++i) {
                    size_t idx = i % vec.size();
                    if (vec[idx] == static_cast<int>(idx)) {
                        ++success_count;
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_TRUE(success_count > 0, "Should have successful reads");
    }
    END_SUBTEST;
    
    return get_subtest_tracker().all_passed();
}

bool test_SmallVector_ThreadSafety_ConcurrentWrites() {
    SUBTEST("Thread safety: concurrent writes") {
        SmallVector<int, 8, 2, std::allocator<int>, ReadWriteLock> vec;
        
        std::vector<std::thread> threads;
        
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&vec, t]() {
                for (int i = 0; i < 25; ++i) {
                    vec.push_back(t * 100 + i);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_EQ(vec.size(), 100, "Should have 100 elements");
    }
    END_SUBTEST;
    
    return get_subtest_tracker().all_passed();
}

bool test_SmallVector_ThreadSafety_MixedReadWrite() {
    SUBTEST("Thread safety: mixed read/write") {
        SmallVector<int, 8, 2, std::allocator<int>, ReadWriteLock> vec;
        
        for (int i = 0; i < 50; ++i) {
            vec.push_back(i);
        }
        
        std::atomic<bool> done{false};
        std::vector<std::thread> threads;
        
        for (int t = 0; t < 2; ++t) {
            threads.emplace_back([&vec]() {
                for (int i = 0; i < 25; ++i) {
                    vec.push_back(i);
                    std::this_thread::yield();
                }
            });
        }
        
        for (int t = 0; t < 2; ++t) {
            threads.emplace_back([&vec, &done]() {
                while (!done) {
                    if (!vec.empty()) {
                        volatile int val = vec[0];
                        (void)val;
                    }
                    std::this_thread::yield();
                }
            });
        }
        
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        done = true;
        
        ASSERT_TRUE(vec.size() >= 50, "Should have at least initial elements");
    }
    END_SUBTEST;
    
    return get_subtest_tracker().all_passed();
}

bool test_SmallVector_Serialization_Basic() {
    SUBTEST("Serialization: basic round-trip") {
        SmallVector<int, 4> original = {1, 2, 3, 4, 5, 6, 7, 8};
        
        std::ostringstream oss;
        {
            BinaryOutputArchive oar(oss);
            const_cast<const SmallVector<int, 4>&>(original).serialize(oar);
        }
        
        SmallVector<int, 4> loaded;
        std::istringstream iss(oss.str());
        {
            BinaryInputArchive iar(iss);
            loaded.serialize(iar);
        }
        
        ASSERT_EQ(loaded.size(), original.size(), "Size should match");
        for (size_t i = 0; i < original.size(); ++i) {
            ASSERT_EQ(loaded[i], original[i], "Element should match");
        }
    }
    END_SUBTEST;
    
    return get_subtest_tracker().all_passed();
}

bool test_SmallVector_Serialization_Empty() {
    SUBTEST("Serialization: empty vector") {
        SmallVector<int, 4> original;
        
        std::ostringstream oss;
        {
            BinaryOutputArchive oar(oss);
            const_cast<const SmallVector<int, 4>&>(original).serialize(oar);
        }
        
        SmallVector<int, 4> loaded;
        loaded.push_back(999);
        
        std::istringstream iss(oss.str());
        {
            BinaryInputArchive iar(iss);
            loaded.serialize(iar);
        }
        
        ASSERT_TRUE(loaded.empty(), "Should be empty");
    }
    END_SUBTEST;
    
    return get_subtest_tracker().all_passed();
}

bool test_SmallVector_Serialization_LargeVector() {
    SUBTEST("Serialization: large vector (heap storage)") {
        SmallVector<int, 4> original;
        for (int i = 0; i < 100; ++i) {
            original.push_back(i);
        }
        
        std::ostringstream oss;
        {
            BinaryOutputArchive oar(oss);
            const_cast<const SmallVector<int, 4>&>(original).serialize(oar);
        }
        
        SmallVector<int, 4> loaded;
        std::istringstream iss(oss.str());
        {
            BinaryInputArchive iar(iss);
            loaded.serialize(iar);
        }
        
        ASSERT_EQ(loaded.size(), original.size(), "Size should match");
        for (size_t i = 0; i < original.size(); ++i) {
            ASSERT_EQ(loaded[i], original[i], "Element should match");
        }
    }
    END_SUBTEST;
    
    return get_subtest_tracker().all_passed();
}

bool test_SmallVector_CustomGrowthFactor() {
    SUBTEST("Custom growth factor: 3x growth") {
        SmallVector<int, 4, 3> vec;
        
        for (int i = 0; i < 5; ++i) {
            vec.push_back(i);
        }
        
        ASSERT_TRUE(vec.capacity() >= 12, "Should grow by factor of 3");
    }
    END_SUBTEST;
    
    return get_subtest_tracker().all_passed();
}

void benchmark_SmallVector_InlineVsHeap() {
    std::cout << "\n=== SmallVector Inline vs Heap Benchmarks ===\n";
    
    const size_t iterations = 1000000;
    
    auto inline_time = measure_perf([&]() {
        SmallVector<int, 8> vec;
        for (int i = 0; i < 4; ++i) {
            vec.push_back(i);
        }
    }, iterations, 10);
    std::cout << "Inline (4 elements): " << format_time(inline_time) << "\n";
    
    auto heap_time = measure_perf([&]() {
        SmallVector<int, 8> vec;
        for (int i = 0; i < 20; ++i) {
            vec.push_back(i);
        }
    }, iterations, 10);
    std::cout << "Heap (20 elements): " << format_time(heap_time) << "\n";
    
    auto std_vec_time = measure_perf([&]() {
        std::vector<int> vec;
        for (int i = 0; i < 4; ++i) {
            vec.push_back(i);
        }
    }, iterations, 10);
    std::cout << "std::vector (4 elements): " << format_time(std_vec_time) << "\n";
    
    std::cout << "SmallVector inline speedup: " 
              << (std_vec_time / inline_time) << "x\n";
}

void benchmark_SmallVector_GrowthFactors() {
    std::cout << "\n=== Growth Factor Benchmarks ===\n";
    
    const size_t iterations = 100000;
    
    auto growth_2x = measure_perf([&]() {
        SmallVector<int, 4, 2> vec;
        for (int i = 0; i < 100; ++i) {
            vec.push_back(i);
        }
    }, iterations, 10);
    std::cout << "Growth factor 2x: " << format_time(growth_2x) << "\n";
    
    auto growth_3x = measure_perf([&]() {
        SmallVector<int, 4, 3> vec;
        for (int i = 0; i < 100; ++i) {
            vec.push_back(i);
        }
    }, iterations, 10);
    std::cout << "Growth factor 3x: " << format_time(growth_3x) << "\n";
}

bool test_SmallVector_Enhanced() {
    PRINT_HEADER(SMALLVECTOR ENHANCED TESTS)
    
    TestRunner runner;
    
    RUN_TEST(runner, test_SmallVector_ExceptionSafety_InsertMultiple);
    RUN_TEST(runner, test_SmallVector_ExceptionSafety_InsertRange);
    RUN_TEST(runner, test_SmallVector_ExceptionSafety_AssignCount);
    RUN_TEST(runner, test_SmallVector_ExceptionSafety_AssignRange);
    RUN_TEST(runner, test_SmallVector_ExceptionSafety_Resize);
    RUN_TEST(runner, test_SmallVector_ExceptionSafety_Emplace);
    
    RUN_TEST(runner, test_SmallVector_ThreadSafety_ConcurrentReads);
    RUN_TEST(runner, test_SmallVector_ThreadSafety_ConcurrentWrites);
    RUN_TEST(runner, test_SmallVector_ThreadSafety_MixedReadWrite);
    
    RUN_TEST(runner, test_SmallVector_Serialization_Basic);
    RUN_TEST(runner, test_SmallVector_Serialization_Empty);
    RUN_TEST(runner, test_SmallVector_Serialization_LargeVector);
    
    RUN_TEST(runner, test_SmallVector_CustomGrowthFactor);
    
    benchmark_SmallVector_InlineVsHeap();
    benchmark_SmallVector_GrowthFactors();
    
    return runner.print_summary() == 0;
}

}
