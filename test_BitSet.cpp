#include <iostream>

#include "BitSet.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_BitSet.h"
#endif

namespace fat_p::testing
{

bool test_bit_set_basic_operations() {
    BitSet<64> bits;
    
    SIMPLE_ASSERT(bits.none(), "Should start with no bits set");
    
    bits.set(5);
    SIMPLE_ASSERT(bits.test(5), "Bit 5 should be set");
    SIMPLE_ASSERT(!bits.test(6), "Bit 6 should not be set");
    
    bits.clear(5);
    SIMPLE_ASSERT(!bits.test(5), "Bit 5 should be cleared");
    
    return true;
}

bool test_bit_set_bulk_operations() {
    BitSet<128> bits;
    
    bits.set_all();
    SIMPLE_ASSERT(bits.all(), "All bits should be set");
    SIMPLE_ASSERT(bits.count() == 128, "Should have 128 bits set");
    
    bits.clear_all();
    SIMPLE_ASSERT(bits.none(), "No bits should be set");
    SIMPLE_ASSERT(bits.count() == 0, "Should have 0 bits set");
    
    bits.set(10);
    bits.set(20);
    bits.set(30);
    SIMPLE_ASSERT(bits.count() == 3, "Should have 3 bits set");
    
    bits.flip_all();
    SIMPLE_ASSERT(bits.count() == 125, "Should have 125 bits set");
    
    return true;
}

bool test_bit_set_find_operations() {
    BitSet<256> bits;
    
    bits.set(10);
    bits.set(50);
    bits.set(200);
    
    SIMPLE_ASSERT(bits.find_first() == 10, "First bit should be 10");
    SIMPLE_ASSERT(bits.find_next(10) == 50, "Next after 10 should be 50");
    SIMPLE_ASSERT(bits.find_next(50) == 200, "Next after 50 should be 200");
    SIMPLE_ASSERT(bits.find_next(200) == 256, "No next after 200");
    
    return true;
}

bool test_bit_set_iteration() {
    BitSet<128> bits;
    
    bits.set(5);
    bits.set(15);
    bits.set(25);
    bits.set(100);
    
    std::vector<size_t> indices;
    for (size_t idx : bits) {
        indices.push_back(idx);
    }
    
    SIMPLE_ASSERT(indices.size() == 4, "Should iterate over 4 bits");
    SIMPLE_ASSERT(indices[0] == 5, "First should be 5");
    SIMPLE_ASSERT(indices[1] == 15, "Second should be 15");
    SIMPLE_ASSERT(indices[2] == 25, "Third should be 25");
    SIMPLE_ASSERT(indices[3] == 100, "Fourth should be 100");
    
    return true;
}

bool test_bit_set_bitwise_operations() {
    BitSet<64> a, b;
    
    a.set(1);
    a.set(2);
    a.set(3);
    
    b.set(2);
    b.set(3);
    b.set(4);
    
    auto and_result = a & b;
    SIMPLE_ASSERT(and_result.count() == 2, "AND should have 2 bits (2,3)");
    SIMPLE_ASSERT(and_result.test(2) && and_result.test(3), "Should have bits 2,3");
    
    auto or_result = a | b;
    SIMPLE_ASSERT(or_result.count() == 4, "OR should have 4 bits");
    
    auto xor_result = a ^ b;
    SIMPLE_ASSERT(xor_result.count() == 2, "XOR should have 2 bits (1,4)");
    SIMPLE_ASSERT(xor_result.test(1) && xor_result.test(4), "Should have bits 1,4");
    
    return true;
}

void benchmark_bitset() {
    std::cout << "\n" << colors::cyan() << "BitSet Benchmarks:" << colors::reset() << "\n\n";
    
    BitSet<1024> bits;
    
    // Benchmark set
    double set_time = measure_perf([&bits, i=0]() mutable {
        bits.set(i % 1024);
        ++i;
    }, 100000, 1000);
    std::cout << "Set bit: " << format_time(set_time) << "\n";
    
    // Benchmark test
    double test_time = measure_perf([&bits]() {
        bool result = bits.test(512);
        DoNotOptimize(result);
    }, 100000, 1000);
    std::cout << "Test bit: " << format_time(test_time) << "\n";
    
    // Benchmark count (uses popcnt)
    bits.set_all();
    double count_time = measure_perf([&bits]() {
        size_t c = bits.count();
        DoNotOptimize(c);
    }, 10000, 100);
    std::cout << "Count bits (popcnt): " << format_time(count_time) << "\n";
    
    // Benchmark bitwise operations
    BitSet<1024> other;
    other.set_all();
    double and_time = measure_perf([&bits, &other]() {
        auto result = bits & other;
        DoNotOptimize(result);
    }, 10000, 100);
    std::cout << "Bitwise AND: " << format_time(and_time) << "\n";
}

bool test_BitSet() {

    PRINT_HEADER(BIT SET)

    TestRunner runner;

    RUN_TEST(runner, bit_set_basic_operations);
    RUN_TEST(runner, bit_set_bulk_operations);
    RUN_TEST(runner, bit_set_find_operations);
    RUN_TEST(runner, bit_set_iteration);
    RUN_TEST(runner, bit_set_bitwise_operations);

    benchmark_bitset();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_BitSet() ? 0 : 1;
}
#endif
