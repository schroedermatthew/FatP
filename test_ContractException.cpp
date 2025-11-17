/**
 * @file test_ContractException.cpp
 * @brief Comprehensive unit tests for ContractException.h v2.0
 * 
 * Tests all features:
 * - Basic exception throwing and catching
 * - Polymorphic base class functionality
 * - Message formatting and prefixes
 * - Category introspection
 * - Standard exception compatibility
 * - Thread safety
 * - Integration with enforce.h patterns
 * 
 * Test Configuration:
 * - Processor: Intel Core i7-8850H @ 2.60GHz
 * - RAM: 32GB
 * - C++ Standard: C++17
 * - Build Modes: Debug and Release
 * 
 * Compilation:
 * - Debug:   g++ -std=c++17 -g test_ContractException.cpp -o test_contract_exception_debug
 * - Release: g++ -std=c++17 -O3 -DNDEBUG test_ContractException.cpp -o test_contract_exception_release
 */

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <exception>
#include <stdexcept>
#include <typeinfo>
#include <sstream>
#include <climits>

// Include the updated ContractException header
#include "ContractException.h"
#include "FatPTest.h"

namespace fat_p::testing {

using namespace fat_p;

// =============================================================================
// Test Suite 1: Basic Functionality
// =============================================================================

bool test_logic_contract_error_basic() {
    try {
        throw LogicContractError("Test logic error");
        SIMPLE_ASSERT(false, "Should have thrown LogicContractError");
    } catch (const LogicContractError& e) {
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.find("Contract Violation:") != std::string::npos,
                      "Message contains 'Contract Violation:' prefix");
        SIMPLE_ASSERT(msg.find("Test logic error") != std::string::npos,
                      "Message contains user text");
    }
    return true;
}

bool test_runtime_contract_error_basic() {
    try {
        throw RuntimeContractError("Test runtime error");
        SIMPLE_ASSERT(false, "Should have thrown RuntimeContractError");
    } catch (const RuntimeContractError& e) {
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.find("Contract Violation:") != std::string::npos,
                      "Message contains 'Contract Violation:' prefix");
        SIMPLE_ASSERT(msg.find("Test runtime error") != std::string::npos,
                      "Message contains user text");
    }
    return true;
}

bool test_alloc_contract_error_basic() {
    try {
        throw AllocContractError("Stack overflow detected");
        SIMPLE_ASSERT(false, "Should have thrown AllocContractError");
    } catch (const AllocContractError& e) {
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.find("Contract Violation:") != std::string::npos,
                      "Message contains 'Contract Violation:' prefix");
        SIMPLE_ASSERT(msg.find("Bad Allocation") != std::string::npos,
                      "Message contains 'Bad Allocation'");
        SIMPLE_ASSERT(msg.find("Stack overflow detected") != std::string::npos,
                      "Message contains user text");
        
        // CRITICAL: Verify no double prefix
        std::string::size_type first_pos = msg.find("Contract Violation:");
        std::string::size_type second_pos = msg.find("Contract Violation:", first_pos + 1);
        SIMPLE_ASSERT(second_pos == std::string::npos,
                      "Message should NOT contain duplicate 'Contract Violation:' prefix");
    }
    return true;
}

bool test_domain_contract_error() {
    try {
        throw DomainContractError("sqrt of negative number");
        SIMPLE_ASSERT(false, "Should have thrown DomainContractError");
    } catch (const DomainContractError& e) {
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.find("sqrt of negative number") != std::string::npos,
                      "Message contains user text");
    } catch (const std::domain_error&) {
        // Also acceptable (inheritance check)
    }
    return true;
}

bool test_out_of_range_contract_error() {
    try {
        throw OutOfRangeContractError("Index 10 out of bounds [0, 5)");
        SIMPLE_ASSERT(false, "Should have thrown OutOfRangeContractError");
    } catch (const OutOfRangeContractError& e) {
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.find("Index 10") != std::string::npos,
                      "Message contains index info");
    } catch (const std::out_of_range&) {
        // Also acceptable (inheritance check)
    }
    return true;
}

bool test_overflow_underflow_contract_errors() {
    // Test OverflowContractError
    try {
        throw OverflowContractError("Integer overflow: INT_MAX + 1");
        SIMPLE_ASSERT(false, "Should have thrown OverflowContractError");
    } catch (const OverflowContractError& e) {
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.find("overflow") != std::string::npos,
                      "Message contains 'overflow'");
    }
    
    // Test UnderflowContractError
    try {
        throw UnderflowContractError("Integer underflow: INT_MIN - 1");
        SIMPLE_ASSERT(false, "Should have thrown UnderflowContractError");
    } catch (const UnderflowContractError& e) {
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.find("underflow") != std::string::npos,
                      "Message contains 'underflow'");
    }
    
    return true;
}

bool test_invalid_argument_contract_error() {
    try {
        throw InvalidArgumentContractError("Null pointer argument");
        SIMPLE_ASSERT(false, "Should have thrown InvalidArgumentContractError");
    } catch (const InvalidArgumentContractError& e) {
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.find("Null pointer") != std::string::npos,
                      "Message contains error details");
    } catch (const std::invalid_argument&) {
        // Also acceptable (inheritance check)
    }
    return true;
}

// =============================================================================
// Test Suite 2: Polymorphic Base Class
// =============================================================================

bool test_polymorphic_catch_base() {
    bool caught_as_base = false;
    
    // Test catching LogicContractError as ContractViolationBase
    try {
        throw LogicContractError("Logic error for polymorphic test");
    } catch (const ContractViolationBase& e) {
        caught_as_base = true;
        SIMPLE_ASSERT(std::string(e.message()).find("Logic error") != std::string::npos,
                      "Polymorphic catch preserves message");
    }
    
    SIMPLE_ASSERT(caught_as_base, "Should have caught as ContractViolationBase");
    return true;
}

bool test_polymorphic_catch_all_types() {
    std::vector<std::string> categories_caught;
    
    // Test all exception types can be caught polymorphically
    try {
        throw LogicContractError("test");
    } catch (const ContractViolationBase& e) {
        categories_caught.push_back(e.category());
    }
    
    try {
        throw RuntimeContractError("test");
    } catch (const ContractViolationBase& e) {
        categories_caught.push_back(e.category());
    }
    
    try {
        throw AllocContractError("test");
    } catch (const ContractViolationBase& e) {
        categories_caught.push_back(e.category());
    }
    
    try {
        throw DomainContractError("test");
    } catch (const ContractViolationBase& e) {
        categories_caught.push_back(e.category());
    }
    
    SIMPLE_ASSERT(categories_caught.size() == 4,
                  "Should have caught all 4 exception types polymorphically");
    
    return true;
}

bool test_category_method() {
    // Test LogicContractError category
    try {
        throw LogicContractError("test");
    } catch (const ContractViolationBase& e) {
        SIMPLE_ASSERT(std::string(e.category()) == "Logic",
                      "LogicContractError should return 'Logic' category");
    }
    
    // Test RuntimeContractError category
    try {
        throw RuntimeContractError("test");
    } catch (const ContractViolationBase& e) {
        SIMPLE_ASSERT(std::string(e.category()) == "Runtime",
                      "RuntimeContractError should return 'Runtime' category");
    }
    
    // Test AllocContractError category
    try {
        throw AllocContractError("test");
    } catch (const ContractViolationBase& e) {
        SIMPLE_ASSERT(std::string(e.category()) == "Allocation",
                      "AllocContractError should return 'Allocation' category");
    }
    
    return true;
}

// =============================================================================
// Test Suite 3: Standard Exception Compatibility
// =============================================================================

bool test_logic_error_inheritance() {
    // LogicContractError should be catchable as std::logic_error
    bool caught_as_logic_error = false;
    
    try {
        throw LogicContractError("test");
    } catch (const std::logic_error& e) {
        caught_as_logic_error = true;
        SIMPLE_ASSERT(std::string(e.what()).find("test") != std::string::npos,
                      "Message preserved when caught as std::logic_error");
    }
    
    SIMPLE_ASSERT(caught_as_logic_error,
                  "LogicContractError should be catchable as std::logic_error");
    return true;
}

bool test_runtime_error_inheritance() {
    // RuntimeContractError should be catchable as std::runtime_error
    bool caught_as_runtime_error = false;
    
    try {
        throw RuntimeContractError("test");
    } catch (const std::runtime_error& e) {
        caught_as_runtime_error = true;
        SIMPLE_ASSERT(std::string(e.what()).find("test") != std::string::npos,
                      "Message preserved when caught as std::runtime_error");
    }
    
    SIMPLE_ASSERT(caught_as_runtime_error,
                  "RuntimeContractError should be catchable as std::runtime_error");
    return true;
}

bool test_bad_alloc_inheritance() {
    // AllocContractError should be catchable as std::bad_alloc
    bool caught_as_bad_alloc = false;
    
    try {
        throw AllocContractError("Custom allocation failure");
    } catch (const std::bad_alloc& e) {
        caught_as_bad_alloc = true;
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.find("allocation") != std::string::npos ||
                      msg.find("Allocation") != std::string::npos,
                      "Message should mention allocation when caught as std::bad_alloc");
    }
    
    SIMPLE_ASSERT(caught_as_bad_alloc,
                  "AllocContractError should be catchable as std::bad_alloc");
    return true;
}

bool test_exception_hierarchy() {
    // All contract exceptions should be catchable as std::exception
    int exception_count = 0;
    
    try {
        throw LogicContractError("test1");
    } catch (const std::exception&) {
        ++exception_count;
    }
    
    try {
        throw RuntimeContractError("test2");
    } catch (const std::exception&) {
        ++exception_count;
    }
    
    try {
        throw AllocContractError("test3");
    } catch (const std::exception&) {
        ++exception_count;
    }
    
    SIMPLE_ASSERT(exception_count == 3,
                  "All contract exceptions should be catchable as std::exception");
    return true;
}

// =============================================================================
// Test Suite 4: Message Formatting
// =============================================================================

bool test_message_with_variables() {
    int value = 42;
    std::string var_name = "x";
    
    try {
        std::ostringstream oss;
        oss << "Invalid value for " << var_name << ": " << value;
        throw LogicContractError(oss.str());
    } catch (const LogicContractError& e) {
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.find("42") != std::string::npos,
                      "Message contains interpolated value");
        SIMPLE_ASSERT(msg.find("x") != std::string::npos,
                      "Message contains variable name");
    }
    
    return true;
}

bool test_message_with_special_characters() {
    try {
        throw RuntimeContractError("Path: /tmp/file\nLine: 10\tColumn: 5");
    } catch (const RuntimeContractError& e) {
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.find("/tmp/file") != std::string::npos,
                      "Message handles path with slash");
        SIMPLE_ASSERT(msg.find("Line:") != std::string::npos,
                      "Message handles newline in string");
        SIMPLE_ASSERT(msg.find("Column:") != std::string::npos,
                      "Message handles tab in string");
    }
    
    return true;
}

bool test_long_message() {
    std::string long_msg(1000, 'x');
    try {
        throw LogicContractError(long_msg);
    } catch (const LogicContractError& e) {
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.length() > 1000,
                      "Long message preserved (with prefix)");
        SIMPLE_ASSERT(msg.find('x') != std::string::npos,
                      "Long message content preserved");
    }
    
    return true;
}

bool test_empty_message() {
    try {
        throw LogicContractError("");
    } catch (const LogicContractError& e) {
        std::string msg = e.what();
        SIMPLE_ASSERT(msg.find("Contract Violation:") != std::string::npos,
                      "Empty message still gets prefix");
    }
    
    return true;
}

// =============================================================================
// Test Suite 5: Thread Safety
// =============================================================================

bool test_concurrent_exception_throwing() {
    const int num_threads = 4;
    const int iterations = 1000;
    std::atomic<int> logic_count{0};
    std::atomic<int> runtime_count{0};
    std::atomic<int> alloc_count{0};
    
    auto thread_func = [&](int thread_id) {
        for (int i = 0; i < iterations; ++i) {
            try {
                switch (i % 3) {
                    case 0:
                        throw LogicContractError("Thread " + std::to_string(thread_id));
                    case 1:
                        throw RuntimeContractError("Thread " + std::to_string(thread_id));
                    case 2:
                        throw AllocContractError("Thread " + std::to_string(thread_id));
                }
            } catch (const ContractViolationBase& e) {
                std::string cat = e.category();
                if (cat == "Logic") ++logic_count;
                else if (cat == "Runtime") ++runtime_count;
                else if (cat == "Allocation") ++alloc_count;
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(thread_func, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    int base = iterations / 3;
    int rem = iterations % 3;
    int expected_logic = num_threads * (base + (rem > 0 ? 1 : 0));
    int expected_runtime = num_threads * (base + (rem > 1 ? 1 : 0));
    int expected_alloc = num_threads * base;
    SIMPLE_ASSERT(logic_count.load() == expected_logic,
                  "Logic exceptions counted correctly in concurrent test");
    SIMPLE_ASSERT(runtime_count.load() == expected_runtime,
                  "Runtime exceptions counted correctly in concurrent test");
    SIMPLE_ASSERT(alloc_count.load() == expected_alloc,
                  "Alloc exceptions counted correctly in concurrent test");
    
    return true;
}

// =============================================================================
// Test Suite 6: Integration Scenarios
// =============================================================================

bool test_factory_error_pattern() {
    // Simulate Factory.h error handling pattern
    auto create_object = [](const std::string& key) {
        if (key.empty()) {
            throw InvalidArgumentContractError("Factory key cannot be empty");
        }
        if (key == "invalid") {
            throw RuntimeContractError("Factory: Unknown key '" + key + "'");
        }
        // Success case...
        return 42;
    };
    
    // Test invalid argument
    try {
        create_object("");
        SIMPLE_ASSERT(false, "Should have thrown InvalidArgumentContractError");
    } catch (const InvalidArgumentContractError& e) {
        SIMPLE_ASSERT(std::string(e.what()).find("empty") != std::string::npos,
                      "Factory error message is descriptive");
    }
    
    // Test unknown key
    try {
        create_object("invalid");
        SIMPLE_ASSERT(false, "Should have thrown RuntimeContractError");
    } catch (const RuntimeContractError& e) {
        SIMPLE_ASSERT(std::string(e.what()).find("Unknown key") != std::string::npos,
                      "Factory error message contains key info");
    }
    
    return true;
}

bool test_checked_arithmetic_pattern() {
    // Simulate CheckedArithmetic.h overflow detection pattern
    auto checked_add = [](int a, int b) -> int {
        if (a > 0 && b > INT_MAX - a) {
            throw OverflowContractError("Integer overflow in addition");
        }
        return a + b;
    };
    
    try {
        checked_add(INT_MAX, 1);
        SIMPLE_ASSERT(false, "Should have thrown OverflowContractError");
    } catch (const OverflowContractError& e) {
        SIMPLE_ASSERT(std::string(e.category()) == "Runtime",
                      "Overflow error has Runtime category (std::overflow_error inherits runtime_error)");
    }
    
    return true;
}

bool test_allocator_pattern() {
    // Simulate custom allocator failure pattern
    auto allocate = [](size_t size) -> void* {
        constexpr size_t MAX_SIZE = 1024 * 1024; // 1 MB
        if (size > MAX_SIZE) {
            throw AllocContractError("Allocation size " + std::to_string(size) + 
                                    " exceeds limit " + std::to_string(MAX_SIZE));
        }
        // Simulate out of memory
        if (size > 0) {
            throw AllocContractError("Out of memory");
        }
        return nullptr;
    };
    
    // Test size limit
    try {
        allocate(2 * 1024 * 1024);
        SIMPLE_ASSERT(false, "Should have thrown AllocContractError for size limit");
    } catch (const std::bad_alloc& e) {
        // Should be catchable as std::bad_alloc
        SIMPLE_ASSERT(std::string(e.what()).find("exceeds limit") != std::string::npos,
                      "Allocation error message is descriptive");
    }
    
    // Test OOM
    try {
        allocate(100);
        SIMPLE_ASSERT(false, "Should have thrown AllocContractError for OOM");
    } catch (const AllocContractError& e) {
        SIMPLE_ASSERT(std::string(e.what()).find("Out of memory") != std::string::npos,
                      "OOM error message is clear");
        SIMPLE_ASSERT(std::string(e.category()) == "Allocation",
                      "AllocContractError has Allocation category");
    }
    
    return true;
}

// =============================================================================
// Test Runner
// =============================================================================

bool test_ContractException() {

    PRINT_HEADER(CONTRACT EXCEPTION)

    TestRunner runner;
 
    runner.run_test("logic_contract_error_basic", test_logic_contract_error_basic);
    runner.run_test("runtime_contract_error_basic", test_runtime_contract_error_basic);
    runner.run_test("alloc_contract_error_basic", test_alloc_contract_error_basic);
    runner.run_test("domain_contract_error", test_domain_contract_error);
    runner.run_test("out_of_range_contract_error", test_out_of_range_contract_error);
    runner.run_test("overflow_underflow_contract_errors", test_overflow_underflow_contract_errors);
    runner.run_test("invalid_argument_contract_error", test_invalid_argument_contract_error);
    
    // Test Suite 2: Polymorphic Base Class
    std::cout << "\n" << colors::cyan() << "Test Suite 2: Polymorphic Base Class" 
              << colors::reset() << "\n";
    runner.run_test("polymorphic_catch_base", test_polymorphic_catch_base);
    runner.run_test("polymorphic_catch_all_types", test_polymorphic_catch_all_types);
    runner.run_test("category_method", test_category_method);
    
    // Test Suite 3: Standard Exception Compatibility
    std::cout << "\n" << colors::cyan() << "Test Suite 3: Standard Exception Compatibility" 
              << colors::reset() << "\n";
    runner.run_test("logic_error_inheritance", test_logic_error_inheritance);
    runner.run_test("runtime_error_inheritance", test_runtime_error_inheritance);
    runner.run_test("bad_alloc_inheritance", test_bad_alloc_inheritance);
    runner.run_test("exception_hierarchy", test_exception_hierarchy);
    
    // Test Suite 4: Message Formatting
    std::cout << "\n" << colors::cyan() << "Test Suite 4: Message Formatting" 
              << colors::reset() << "\n";
    runner.run_test("message_with_variables", test_message_with_variables);
    runner.run_test("message_with_special_characters", test_message_with_special_characters);
    runner.run_test("long_message", test_long_message);
    runner.run_test("empty_message", test_empty_message);
    
    // Test Suite 5: Thread Safety
    std::cout << "\n" << colors::cyan() << "Test Suite 5: Thread Safety" 
              << colors::reset() << "\n";
    runner.run_test("concurrent_exception_throwing", test_concurrent_exception_throwing);
    
    // Test Suite 6: Integration Scenarios
    std::cout << "\n" << colors::cyan() << "Test Suite 6: Integration Scenarios" 
              << colors::reset() << "\n";
    runner.run_test("factory_error_pattern", test_factory_error_pattern);
    runner.run_test("checked_arithmetic_pattern", test_checked_arithmetic_pattern);
    runner.run_test("allocator_pattern", test_allocator_pattern);
    
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing