/**
 * @file test_StrongId.cpp
 * @brief Comprehensive unit tests for StrongId template
 * 
 * Tests cover:
 * - Basic functionality (construction, accessors, comparison)
 * - Arithmetic operations (all operators)
 * - Bitwise operations
 * - Thread-safe operations (assignment, locking)
 * - Modular arithmetic policy with division
 * - CheckPolicy validation
 * - Expected-based safe creation
 * - Swap functionality
 * - Hash support for containers
 * - AtomicStrongId usage
 * - Performance benchmarks
 */

#include "StrongId.h"
#include "test_Utilities.h"
#include <unordered_map>
#include <thread>
#include <vector>
#include <atomic>

namespace cpp_utilities::testing
{

// --- Test Tags ---
struct UserIdTag {};
struct TransactionIdTag {};
struct ProductIdTag {};
struct ModularIdTag {};

// --- Type Aliases ---
using UserId = StrongId<int, UserIdTag>;
using TransactionId = StrongId<long, TransactionIdTag>;
using ProductId = StrongId<int, ProductIdTag, PositiveCheckPolicy>;
using ModularId = StrongId<int, ModularIdTag, NoCheckPolicy, SingleThreadedPolicy, ModularOpPolicy<7>::Policy>;

// Thread-safe versions
using ThreadSafeUserId = StrongId<int, UserIdTag, NoCheckPolicy, SharedMutexPolicy>;
using ThreadSafeProductId = StrongId<int, ProductIdTag, PositiveCheckPolicy, SharedMutexPolicy>;

// =============================================================================
// Basic Functionality Tests
// =============================================================================

TEST_CASE(default_constructor) {
    UserId id;
    ASSERT_EQ(id.get(), 0, "Default constructor should initialize to 0");
    return true;
}

TEST_CASE(explicit_constructor) {
    UserId id(42);
    ASSERT_EQ(id.get(), 42, "Explicit constructor should set value");
    return true;
}

TEST_CASE(default_constructor_with_check_policy) {
    // Default PositiveCheckPolicy allows 0
    ProductId id;
    ASSERT_EQ(id.get(), 0, "Default constructor should work with check policy");
    return true;
}

TEST_CASE(type_safety) {
    UserId user_id(100);
    TransactionId trans_id(100);
    
    // These should not compile (different types):
    // user_id = trans_id;  // Error: different types
    // bool eq = (user_id == trans_id);  // Error: different types
    
    // But same type comparisons work:
    UserId another_user(100);
    ASSERT_TRUE(user_id == another_user, "Same type comparison should work");
    return true;
}

TEST_CASE(get_accessor) {
    UserId id(123);
    ASSERT_EQ(id.get(), 123, "get() should return underlying value");
    return true;
}

TEST_CASE(explicit_cast) {
    UserId id(456);
    int value = static_cast<int>(id);
    ASSERT_EQ(value, 456, "Explicit cast should work");
    return true;
}

// =============================================================================
// Comparison Operator Tests
// =============================================================================

TEST_CASE(equality_comparison) {
    UserId id1(100);
    UserId id2(100);
    UserId id3(200);
    
    ASSERT_TRUE(id1 == id2, "Equal IDs should compare equal");
    ASSERT_FALSE(id1 == id3, "Unequal IDs should not compare equal");
    return true;
}

TEST_CASE(inequality_comparison) {
    UserId id1(100);
    UserId id2(200);
    
    ASSERT_TRUE(id1 != id2, "Unequal IDs should compare not equal");
    ASSERT_FALSE(id1 != id1, "Same ID should not compare not equal");
    return true;
}

TEST_CASE(less_than_comparison) {
    UserId id1(100);
    UserId id2(200);
    
    ASSERT_TRUE(id1 < id2, "Smaller ID should be less than larger");
    ASSERT_FALSE(id2 < id1, "Larger ID should not be less than smaller");
    ASSERT_FALSE(id1 < id1, "ID should not be less than itself");
    return true;
}

TEST_CASE(all_relational_operators) {
    UserId id1(100);
    UserId id2(200);
    
    ASSERT_TRUE(id1 <= id2, "Less-or-equal should work");
    ASSERT_TRUE(id1 <= id1, "Equal IDs should satisfy <=");
    ASSERT_TRUE(id2 > id1, "Greater should work");
    ASSERT_TRUE(id2 >= id1, "Greater-or-equal should work");
    ASSERT_TRUE(id1 >= id1, "Equal IDs should satisfy >=");
    return true;
}

#if __cplusplus >= 202002L
TEST_CASE(spaceship_operator) {
    UserId id1(100);
    UserId id2(200);
    UserId id3(100);
    
    ASSERT_TRUE((id1 <=> id2) < 0, "Spaceship operator: less than");
    ASSERT_TRUE((id2 <=> id1) > 0, "Spaceship operator: greater than");
    ASSERT_TRUE((id1 <=> id3) == 0, "Spaceship operator: equal");
    return true;
}
#endif

// =============================================================================
// Arithmetic Operator Tests
// =============================================================================

TEST_CASE(increment_operators) {
    UserId id(10);
    
    ++id;
    ASSERT_EQ(id.get(), 11, "Pre-increment should work");
    
    UserId id2 = id++;
    ASSERT_EQ(id.get(), 12, "Post-increment should increment");
    ASSERT_EQ(id2.get(), 11, "Post-increment should return old value");
    return true;
}

TEST_CASE(decrement_operators) {
    UserId id(10);
    
    --id;
    ASSERT_EQ(id.get(), 9, "Pre-decrement should work");
    
    UserId id2 = id--;
    ASSERT_EQ(id.get(), 8, "Post-decrement should decrement");
    ASSERT_EQ(id2.get(), 9, "Post-decrement should return old value");
    return true;
}

TEST_CASE(addition_operators) {
    UserId id1(100);
    
    id1 += 50;
    ASSERT_EQ(id1.get(), 150, "Compound addition should work");
    
    UserId id2 = id1 + 25;
    ASSERT_EQ(id2.get(), 175, "Binary addition with scalar should work");
    ASSERT_EQ(id1.get(), 150, "Original should be unchanged");
    
    UserId id3(50);
    UserId id4 = id1 + id3;
    ASSERT_EQ(id4.get(), 200, "Binary addition with StrongId should work");
    return true;
}

TEST_CASE(subtraction_operators) {
    UserId id1(200);
    
    id1 -= 50;
    ASSERT_EQ(id1.get(), 150, "Compound subtraction should work");
    
    UserId id2 = id1 - 25;
    ASSERT_EQ(id2.get(), 125, "Binary subtraction with scalar should work");
    
    UserId id3(50);
    UserId id4 = id1 - id3;
    ASSERT_EQ(id4.get(), 100, "Binary subtraction with StrongId should work");
    return true;
}

TEST_CASE(multiplication_operators) {
    UserId id1(10);
    
    id1 *= 5;
    ASSERT_EQ(id1.get(), 50, "Compound multiplication should work");
    
    UserId id2 = id1 * 2;
    ASSERT_EQ(id2.get(), 100, "Binary multiplication with scalar should work");
    
    UserId id3(3);
    UserId id4 = id1 * id3;
    ASSERT_EQ(id4.get(), 150, "Binary multiplication with StrongId should work");
    return true;
}

TEST_CASE(division_operators) {
    UserId id1(100);
    
    id1 /= 5;
    ASSERT_EQ(id1.get(), 20, "Compound division should work");
    
    UserId id2 = id1 / 2;
    ASSERT_EQ(id2.get(), 10, "Binary division with scalar should work");
    
    UserId id3(2);
    UserId id4 = id1 / id3;
    ASSERT_EQ(id4.get(), 10, "Binary division with StrongId should work");
    return true;
}

TEST_CASE(modulo_operators) {
    UserId id1(17);
    
    id1 %= 5;
    ASSERT_EQ(id1.get(), 2, "Compound modulo should work");
    
    UserId id2(17);
    UserId id3 = id2 % 5;
    ASSERT_EQ(id3.get(), 2, "Binary modulo with scalar should work");
    
    UserId id4(17);
    UserId id5(5);
    UserId id6 = id4 % id5;
    ASSERT_EQ(id6.get(), 2, "Binary modulo with StrongId should work");
    return true;
}

TEST_CASE(unary_operators) {
    UserId id1(42);
    UserId id2 = -id1;
    ASSERT_EQ(id2.get(), -42, "Unary negation should work");
    
    UserId id3 = +id1;
    ASSERT_EQ(id3.get(), 42, "Unary plus should return copy");
    return true;
}

// =============================================================================
// Bitwise Operator Tests
// =============================================================================

TEST_CASE(bitwise_and) {
    UserId id1(0b1111);
    UserId id2(0b1010);
    
    UserId id3 = id1 & id2;
    ASSERT_EQ(id3.get(), 0b1010, "Bitwise AND should work");
    
    id1 &= 0b1100;
    ASSERT_EQ(id1.get(), 0b1100, "Compound bitwise AND should work");
    return true;
}

TEST_CASE(bitwise_or) {
    UserId id1(0b1100);
    UserId id2(0b1010);
    
    UserId id3 = id1 | id2;
    ASSERT_EQ(id3.get(), 0b1110, "Bitwise OR should work");
    
    id1 |= 0b0011;
    ASSERT_EQ(id1.get(), 0b1111, "Compound bitwise OR should work");
    return true;
}

TEST_CASE(bitwise_xor) {
    UserId id1(0b1100);
    UserId id2(0b1010);
    
    UserId id3 = id1 ^ id2;
    ASSERT_EQ(id3.get(), 0b0110, "Bitwise XOR should work");
    
    id1 ^= 0b1111;
    ASSERT_EQ(id1.get(), 0b0011, "Compound bitwise XOR should work");
    return true;
}

TEST_CASE(bitwise_not) {
    UserId id1(0b00001111);
    UserId id2 = ~id1;
    ASSERT_EQ(id2.get(), ~0b00001111, "Bitwise NOT should work");
    return true;
}

TEST_CASE(bit_shifts) {
    UserId id1(1);
    
    id1 <<= 3;
    ASSERT_EQ(id1.get(), 8, "Left shift compound should work");
    
    UserId id2 = id1 << 2;
    ASSERT_EQ(id2.get(), 32, "Left shift binary should work");
    
    id1 >>= 2;
    ASSERT_EQ(id1.get(), 2, "Right shift compound should work");
    
    UserId id3 = id1 >> 1;
    ASSERT_EQ(id3.get(), 1, "Right shift binary should work");
    return true;
}

// =============================================================================
// Modular Arithmetic Policy Tests
// =============================================================================

TEST_CASE(modular_addition) {
    ModularId id1(5);
    ModularId id2(4);
    
    ModularId id3 = id1 + id2;
    ASSERT_EQ(id3.get(), 2, "Modular addition: (5 + 4) % 7 = 2");
    
    ModularId id4(6);
    id4 += 3;
    ASSERT_EQ(id4.get(), 2, "Modular compound addition: (6 + 3) % 7 = 2");
    return true;
}

TEST_CASE(modular_subtraction) {
    ModularId id1(2);
    ModularId id2(5);
    
    ModularId id3 = id1 - id2;
    ASSERT_EQ(id3.get(), 4, "Modular subtraction: (2 - 5) % 7 = 4");
    return true;
}

TEST_CASE(modular_multiplication) {
    ModularId id1(3);
    ModularId id2(5);
    
    ModularId id3 = id1 * id2;
    ASSERT_EQ(id3.get(), 1, "Modular multiplication: (3 * 5) % 7 = 1");
    return true;
}

TEST_CASE(modular_division) {
    // Test modular division: (a / b) mod 7 = (a * b^-1) mod 7
    // For prime modulus 7: 2^-1 mod 7 = 4 (because 2 * 4 = 8 ≡ 1 mod 7)
    ModularId id1(6);
    ModularId id2(2);
    
    ModularId id3 = id1 / id2;
    ASSERT_EQ(id3.get(), 3, "Modular division: (6 / 2) % 7 = 3");
    
    // Another test: (1 / 3) mod 7
    // 3^-1 mod 7 = 5 (because 3 * 5 = 15 ≡ 1 mod 7)
    ModularId id4(1);
    ModularId id5(3);
    ModularId id6 = id4 / id5;
    ASSERT_EQ(id6.get(), 5, "Modular division: (1 / 3) % 7 = 5");
    return true;
}

TEST_CASE(modular_negation) {
    ModularId id1(3);
    ModularId id2 = -id1;
    ASSERT_EQ(id2.get(), 4, "Modular negation: -3 % 7 = 4");
    return true;
}

// =============================================================================
// CheckPolicy Tests
// =============================================================================

TEST_CASE(positive_check_policy_valid) {
    ProductId id(42);
    ASSERT_EQ(id.get(), 42u, "Positive value should pass check");
    
    ProductId id2(0);
    ASSERT_EQ(id2.get(), 0u, "Zero should pass positive check");
    return true;
}

TEST_CASE(positive_check_policy_invalid) {
    bool caught = false;
    try {
        ProductId id(-1);  // Should throw
    } catch (const std::exception&) {
        caught = true;
    }
    ASSERT_TRUE(caught, "Negative value should fail positive check");
    return true;
}

TEST_CASE(check_policy_in_default_constructor) {
    // This should work (0 is valid for PositiveCheckPolicy)
    ProductId id;
    ASSERT_EQ(id.get(), 0u, "Default constructor should apply check policy");
    return true;
}

// =============================================================================
// Expected-Based Safe Creation Tests
// =============================================================================

TEST_CASE(expected_create_success) {
    auto result = ProductId::create(100);
    ASSERT_TRUE(result.has_value(), "Valid value should succeed");
    ASSERT_EQ(result.value().get(), 100, "Created ID should have correct value");
    return true;
}

TEST_CASE(expected_create_failure) {
    auto result = ProductId::create(-1);
    ASSERT_FALSE(result.has_value(), "Invalid value should fail");
    ASSERT_TRUE(result.error().find("Negative") != std::string::npos,
                "Error should mention negative value");
    return true;
}

// =============================================================================
// Thread-Safe Assignment Tests
// =============================================================================

TEST_CASE(copy_assignment) {
    UserId id1(100);
    UserId id2(200);
    
    id2 = id1;
    ASSERT_EQ(id2.get(), 100, "Copy assignment should work");
    ASSERT_EQ(id1.get(), 100, "Source should be unchanged");
    return true;
}

TEST_CASE(move_assignment) {
    UserId id1(100);
    UserId id2(200);
    
    id2 = std::move(id1);
    ASSERT_EQ(id2.get(), 100, "Move assignment should work");
    return true;
}

TEST_CASE(self_assignment) {
    UserId id(100);
    id = id;  // Self-assignment
    ASSERT_EQ(id.get(), 100, "Self-assignment should be safe");
    return true;
}

TEST_CASE(threadsafe_copy_assignment) {
    ThreadSafeUserId id1(100);
    ThreadSafeUserId id2(200);
    
    std::vector<std::thread> threads;
    std::atomic<int> errors{0};
    
    // Multiple threads copying
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&id1, &id2, &errors]() {
            try {
                for (int j = 0; j < 100; ++j) {
                    id2 = id1;
                    if (id2.get() != 100) {
                        errors++;
                    }
                }
            } catch (...) {
                errors++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(errors.load(), 0, "Thread-safe copy assignment should not have errors");
    ASSERT_EQ(id2.get(), 100, "Final value should be correct");
    return true;
}

// =============================================================================
// Swap Tests
// =============================================================================

TEST_CASE(member_swap) {
    UserId id1(100);
    UserId id2(200);
    
    id1.swap(id2);
    ASSERT_EQ(id1.get(), 200, "After swap, id1 should have id2's value");
    ASSERT_EQ(id2.get(), 100, "After swap, id2 should have id1's value");
    return true;
}

TEST_CASE(adl_swap) {
    UserId id1(100);
    UserId id2(200);
    
    using std::swap;
    swap(id1, id2);
    ASSERT_EQ(id1.get(), 200, "ADL swap should work");
    ASSERT_EQ(id2.get(), 100, "ADL swap should work");
    return true;
}

TEST_CASE(threadsafe_swap) {
    ThreadSafeUserId id1(100);
    ThreadSafeUserId id2(200);
    
    std::vector<std::thread> threads;
    std::atomic<int> errors{0};
    
    // Multiple threads swapping
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&id1, &id2, &errors]() {
            try {
                for (int j = 0; j < 100; ++j) {
                    id1.swap(id2);
                }
            } catch (...) {
                errors++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(errors.load(), 0, "Thread-safe swap should not have errors");
    // After even number of swaps, values should be back to original or swapped
    ASSERT_TRUE(id1.get() == 100 || id1.get() == 200, "Value should be valid");
    return true;
}

// =============================================================================
// Hash and Container Tests
// =============================================================================

TEST_CASE(hash_function) {
    UserId id1(100);
    UserId id2(100);
    UserId id3(200);
    
    std::hash<UserId> hasher;
    
    ASSERT_EQ(hasher(id1), hasher(id2), "Equal IDs should have equal hashes");
    ASSERT_NE(hasher(id1), hasher(id3), "Different IDs should (likely) have different hashes");
    return true;
}

TEST_CASE(unordered_map_usage) {
    std::unordered_map<UserId, std::string> user_names;
    
    user_names[UserId(1)] = "Alice";
    user_names[UserId(2)] = "Bob";
    user_names[UserId(3)] = "Charlie";
    
    ASSERT_EQ(user_names[UserId(1)], "Alice", "Lookup should work");
    ASSERT_EQ(user_names[UserId(2)], "Bob", "Lookup should work");
    ASSERT_EQ(user_names.size(), 3u, "Size should be correct");
    
    user_names[UserId(1)] = "Alicia";  // Update
    ASSERT_EQ(user_names[UserId(1)], "Alicia", "Update should work");
    ASSERT_EQ(user_names.size(), 3u, "Size should remain 3 after update");
    return true;
}

// =============================================================================
// Thread-Safety Tests with Shared Operations
// =============================================================================

TEST_CASE(threadsafe_concurrent_reads) {
    ThreadSafeUserId id(42);
    std::vector<std::thread> threads;
    std::atomic<int> errors{0};
    
    // Multiple threads reading simultaneously
    for (int i = 0; i < 20; ++i) {
        threads.emplace_back([&id, &errors]() {
            try {
                for (int j = 0; j < 1000; ++j) {
                    int value = id.get();
                    if (value != 42) {
                        errors++;
                    }
                }
            } catch (...) {
                errors++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(errors.load(), 0, "Concurrent reads should be safe");
    return true;
}

TEST_CASE(threadsafe_concurrent_writes) {
    ThreadSafeUserId id(0);
    std::vector<std::thread> threads;
    std::atomic<int> total{0};
    
    // Multiple threads incrementing
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&id, &total]() {
            for (int j = 0; j < 100; ++j) {
                ++id;
                total++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(id.get(), 1000, "Concurrent increments should all be counted");
    return true;
}

TEST_CASE(threadsafe_mixed_operations) {
    ThreadSafeUserId id(1000);
    std::vector<std::thread> threads;
    std::atomic<int> errors{0};
    
    // Readers
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&id, &errors]() {
            try {
                for (int j = 0; j < 500; ++j) {
                    int value = id.get();
                    if (value < 0) errors++;
                }
            } catch (...) {
                errors++;
            }
        });
    }
    
    // Writers
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&id, &errors]() {
            try {
                for (int j = 0; j < 100; ++j) {
                    ++id;
                }
            } catch (...) {
                errors++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(errors.load(), 0, "Mixed operations should be safe");
    ASSERT_EQ(id.get(), 1500, "All writes should be counted");
    return true;
}

// =============================================================================
// Performance Benchmarks
// =============================================================================

void run_strong_id_benchmarks() {
    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << colors::bold() 
        << "=== StrongId Performance Benchmarks ===" 
        << colors::reset() << "\n\n";
    
    // Construction benchmark
    benchmark("StrongId Construction", []() {
        volatile UserId id(42);
    });
    
    // Get accessor benchmark
    UserId id(42);
    benchmark("StrongId get()", [&id]() {
        volatile int x = id.get();
    });
    
    // Arithmetic operations
    benchmark("StrongId Addition", [&id]() {
        volatile UserId result = id + 10;
    });
    
    benchmark("StrongId Multiplication", [&id]() {
        volatile UserId result = id * 2;
    });
    
    // Comparison operations
    UserId id2(100);
    benchmark("StrongId Comparison", [&id, &id2]() {
        volatile bool result = id < id2;
    });
    
    // Modular arithmetic
    ModularId mod_id(5);
    benchmark("Modular Addition", [&mod_id]() {
        volatile ModularId result = mod_id + ModularId(3);
    });
    
    benchmark("Modular Multiplication", [&mod_id]() {
        volatile ModularId result = mod_id * ModularId(4);
    });
    
    benchmark("Modular Division", [&mod_id]() {
        volatile ModularId result = mod_id / ModularId(2);
    });
    
    // Thread-safe operations
    ThreadSafeUserId ts_id(42);
    benchmark("Thread-Safe get()", [&ts_id]() {
        volatile int x = ts_id.get();
    });
    
    benchmark("Thread-Safe Increment", [&ts_id]() {
        ++ts_id;
    });
    
    // Hash benchmark
    std::hash<UserId> hasher;
    benchmark("Hash Calculation", [&id, &hasher]() {
        volatile size_t h = hasher(id);
    });
    
    out << "\n";
}

bool test_StrongId() {

    PRINT_HEADER(STRONG ID)

    TestRunner runner;
    
    auto& config = get_test_config();
    config.verbose = true;
    
    auto& out = *config.output;    
    // Basic Functionality
    out << colors::blue() << "--- Basic Functionality ---" << colors::reset() << "\n";
    RUN_TEST(runner, default_constructor);
    RUN_TEST(runner, explicit_constructor);
    RUN_TEST(runner, default_constructor_with_check_policy);
    RUN_TEST(runner, type_safety);
    RUN_TEST(runner, get_accessor);
    RUN_TEST(runner, explicit_cast);
    
    // Comparison Operators
    out << "\n" << colors::blue() << "--- Comparison Operators ---" << colors::reset() << "\n";
    RUN_TEST(runner, equality_comparison);
    RUN_TEST(runner, inequality_comparison);
    RUN_TEST(runner, less_than_comparison);
    RUN_TEST(runner, all_relational_operators);
#if __cplusplus >= 202002L
    RUN_TEST(runner, spaceship_operator);
#endif
    
    // Arithmetic Operators
    out << "\n" << colors::blue() << "--- Arithmetic Operators ---" << colors::reset() << "\n";
    RUN_TEST(runner, increment_operators);
    RUN_TEST(runner, decrement_operators);
    RUN_TEST(runner, addition_operators);
    RUN_TEST(runner, subtraction_operators);
    RUN_TEST(runner, multiplication_operators);
    RUN_TEST(runner, division_operators);
    RUN_TEST(runner, modulo_operators);
    RUN_TEST(runner, unary_operators);
    
    // Bitwise Operators
    out << "\n" << colors::blue() << "--- Bitwise Operators ---" << colors::reset() << "\n";
    RUN_TEST(runner, bitwise_and);
    RUN_TEST(runner, bitwise_or);
    RUN_TEST(runner, bitwise_xor);
    RUN_TEST(runner, bitwise_not);
    RUN_TEST(runner, bit_shifts);
    
    // Modular Arithmetic
    out << "\n" << colors::blue() << "--- Modular Arithmetic Policy ---" << colors::reset() << "\n";
    RUN_TEST(runner, modular_addition);
    RUN_TEST(runner, modular_subtraction);
    RUN_TEST(runner, modular_multiplication);
    RUN_TEST(runner, modular_division);
    RUN_TEST(runner, modular_negation);
    
    // CheckPolicy
    out << "\n" << colors::blue() << "--- CheckPolicy Validation ---" << colors::reset() << "\n";
    RUN_TEST(runner, positive_check_policy_valid);
    RUN_TEST(runner, positive_check_policy_invalid);
    RUN_TEST(runner, check_policy_in_default_constructor);
    
    // Expected
    out << "\n" << colors::blue() << "--- Expected-Based Safe Creation ---" << colors::reset() << "\n";
    RUN_TEST(runner, expected_create_success);
    RUN_TEST(runner, expected_create_failure);
    
    // Assignment
    out << "\n" << colors::blue() << "--- Assignment Operators ---" << colors::reset() << "\n";
    RUN_TEST(runner, copy_assignment);
    RUN_TEST(runner, move_assignment);
    RUN_TEST(runner, self_assignment);
    RUN_TEST(runner, threadsafe_copy_assignment);
    
    // Swap
    out << "\n" << colors::blue() << "--- Swap Functionality ---" << colors::reset() << "\n";
    RUN_TEST(runner, member_swap);
    RUN_TEST(runner, adl_swap);
    RUN_TEST(runner, threadsafe_swap);
    
    // Hash and Containers
    out << "\n" << colors::blue() << "--- Hash and Containers ---" << colors::reset() << "\n";
    RUN_TEST(runner, hash_function);
    RUN_TEST(runner, unordered_map_usage);
    
    // Thread-Safety
    out << "\n" << colors::blue() << "--- Thread-Safety Tests ---" << colors::reset() << "\n";
    RUN_TEST(runner, threadsafe_concurrent_reads);
    RUN_TEST(runner, threadsafe_concurrent_writes);
    RUN_TEST(runner, threadsafe_mixed_operations);
    
    // Performance Benchmarks
    run_strong_id_benchmarks();
    
    // Summary
    return 0 == runner.print_summary() ? true : false;
}
} // namespace cpp_utilities::testing