#include <iostream>
#include <numeric>

#include "AlignedVector.h"
#include "test_AlignedVector.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

using namespace memory;

bool test_aligned_vector_construction() {
    AlignedVector<int> vec1;
    SIMPLE_ASSERT(vec1.empty(), "Default constructor should create empty vector");
    SIMPLE_ASSERT(vec1.size() == 0, "Size should be 0");
    
    AlignedVector<int> vec2(10);
    SIMPLE_ASSERT(vec2.size() == 10, "Size constructor should create vector of size 10");
    
    AlignedVector<int> vec3(5, 42);
    SIMPLE_ASSERT(vec3.size() == 5, "Should have size 5");
    for (size_t i = 0; i < 5; ++i) {
        SIMPLE_ASSERT(vec3[i] == 42, "All elements should be 42");
    }
    
    AlignedVector<int> vec4 = {1, 2, 3, 4, 5};
    SIMPLE_ASSERT(vec4.size() == 5, "Initializer list should create vector of size 5");
    SIMPLE_ASSERT(vec4[0] == 1 && vec4[4] == 5, "Elements should be initialized correctly");
    
    return true;
}

bool test_aligned_vector_alignment() {
    AlignedVector<float, 64> vec(100);
    
    SIMPLE_ASSERT(vec.is_aligned(), "Data should be aligned");
    SIMPLE_ASSERT(reinterpret_cast<uintptr_t>(vec.data()) % 64 == 0, 
                  "Data pointer should be 64-byte aligned");
    
    AlignedVector<double, 32> vec2(50);
    SIMPLE_ASSERT(reinterpret_cast<uintptr_t>(vec2.data()) % 32 == 0,
                  "Data pointer should be 32-byte aligned");
    
    return true;
}

bool test_aligned_vector_push_back() {
    AlignedVector<int> vec;
    
    for (int i = 0; i < 100; ++i) {
        vec.push_back(i);
    }
    
    SIMPLE_ASSERT(vec.size() == 100, "Should have 100 elements");
    for (int i = 0; i < 100; ++i) {
        SIMPLE_ASSERT(vec[i] == i, "Elements should match");
    }
    
    return true;
}

bool test_aligned_vector_emplace_back() {
    struct Point {
        int x, y;
        Point(int x_, int y_) : x(x_), y(y_) {}
    };
    
    AlignedVector<Point> vec;
    vec.emplace_back(1, 2);
    vec.emplace_back(3, 4);
    
    SIMPLE_ASSERT(vec.size() == 2, "Should have 2 elements");
    SIMPLE_ASSERT(vec[0].x == 1 && vec[0].y == 2, "First point should be (1,2)");
    SIMPLE_ASSERT(vec[1].x == 3 && vec[1].y == 4, "Second point should be (3,4)");
    
    return true;
}

bool test_aligned_vector_resize() {
    AlignedVector<int> vec(10, 5);
    
    vec.resize(20);
    SIMPLE_ASSERT(vec.size() == 20, "Size should be 20 after resize");
    for (size_t i = 0; i < 10; ++i) {
        SIMPLE_ASSERT(vec[i] == 5, "Original elements should remain");
    }
    
    vec.resize(5);
    SIMPLE_ASSERT(vec.size() == 5, "Size should be 5 after shrink");
    
    vec.resize(15, 99);
    SIMPLE_ASSERT(vec.size() == 15, "Size should be 15");
    for (size_t i = 5; i < 15; ++i) {
        SIMPLE_ASSERT(vec[i] == 99, "New elements should be 99");
    }
    
    return true;
}

bool test_aligned_vector_copy_move() {
    AlignedVector<int> vec1 = {1, 2, 3, 4, 5};
    
    // Copy constructor
    AlignedVector<int> vec2(vec1);
    SIMPLE_ASSERT(vec2.size() == 5, "Copy should have same size");
    SIMPLE_ASSERT(vec2[2] == 3, "Copy should have same elements");
    
    // Move constructor
    AlignedVector<int> vec3(std::move(vec2));
    SIMPLE_ASSERT(vec3.size() == 5, "Move should transfer ownership");
    SIMPLE_ASSERT(vec2.size() == 0, "Moved-from vector should be empty");
    
    // Copy assignment
    AlignedVector<int> vec4;
    vec4 = vec1;
    SIMPLE_ASSERT(vec4.size() == 5, "Assignment should copy");
    
    // Move assignment
    AlignedVector<int> vec5;
    vec5 = std::move(vec3);
    SIMPLE_ASSERT(vec5.size() == 5, "Move assignment should transfer");
    SIMPLE_ASSERT(vec3.size() == 0, "Moved-from should be empty");
    
    return true;
}

bool test_aligned_vector_iterators() {
    AlignedVector<int> vec = {1, 2, 3, 4, 5};
    
    int sum = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        sum += *it;
    }
    SIMPLE_ASSERT(sum == 15, "Iterator sum should be 15");
    
    sum = 0;
    for (const auto& val : vec) {
        sum += val;
    }
    SIMPLE_ASSERT(sum == 15, "Range-based for should work");
    
    return true;
}

bool test_aligned_vector_reserve_capacity() {
    AlignedVector<int> vec;
    
    vec.reserve(100);
    SIMPLE_ASSERT(vec.capacity() >= 100, "Capacity should be at least 100");
    SIMPLE_ASSERT(vec.size() == 0, "Size should still be 0");
    
    for (int i = 0; i < 50; ++i) {
        vec.push_back(i);
    }
    
    size_t cap = vec.capacity();
    vec.shrink_to_fit();
    SIMPLE_ASSERT(vec.capacity() <= cap, "Capacity should shrink");
    SIMPLE_ASSERT(vec.size() == 50, "Size should remain 50");
    
    return true;
}

void benchmark_aligned_vector() {
    std::cout << "\n" << colors::cyan() << "AlignedVector Benchmarks:" << colors::reset() << "\n\n";
    
    constexpr size_t N = 10000;
    
    // Benchmark push_back
    double push_time = measure_perf([]() {
        AlignedVector<int> vec;
        for (int i = 0; i < 1000; ++i) {
            vec.push_back(i);
        }
        DoNotOptimize(vec);
    }, 10000, 100);
    
    std::cout << "push_back (1000 elements): " << format_time(push_time) << "\n";
    
    // Benchmark iteration
    AlignedVector<int, 64> vec(N);
    std::iota(vec.begin(), vec.end(), 0);
    
    double iter_time = measure_perf([&vec]() {
        long long sum = 0;
        for (const auto& val : vec) {
            sum += val;
        }
        DoNotOptimize(sum);
    }, 10000, 100);
    
    std::cout << "Iteration sum (" << N << " elements): " << format_time(iter_time) << "\n";
    
    // Benchmark random access
    double access_time = measure_perf([&vec, i = 0]() mutable {
        int val = vec[i % N];
        DoNotOptimize(val);
        ++i;
    }, 100000, 1000);
    
    std::cout << "Random access: " << format_time(access_time) << "\n";
    
    std::cout << "\nAlignment: " << AlignedVector<int, 64>::get_alignment() << " bytes\n";
}

bool test_AlignedVector() {

    PRINT_HEADER(ALIGNED VECTOR)

    TestRunner runner;

    RUN_TEST(runner, aligned_vector_construction);
    RUN_TEST(runner, aligned_vector_alignment);
    RUN_TEST(runner, aligned_vector_push_back);
    RUN_TEST(runner, aligned_vector_emplace_back);
    RUN_TEST(runner, aligned_vector_resize);
    RUN_TEST(runner, aligned_vector_copy_move);
    RUN_TEST(runner, aligned_vector_iterators);
    RUN_TEST(runner, aligned_vector_reserve_capacity);

    benchmark_aligned_vector();

    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing
