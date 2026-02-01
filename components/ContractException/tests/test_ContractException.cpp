/**
 * @file test_ContractException.cpp
 * @brief Comprehensive unit tests for ContractException.h
 *
 * Tests all features:
 * - Basic exception throwing and catching
 * - Polymorphic base class functionality
 * - Message formatting and prefixes
 * - Category introspection
 * - Standard exception compatibility
 * - Stream operator (operator<<)
 * - Move semantics and noexcept guarantees
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
 * - Debug:   g++ -std=c++17 -g -pthread test_ContractException.cpp -o test_debug
 * - Release: g++ -std=c++17 -O3 -DNDEBUG -pthread test_ContractException.cpp -o test_release
 */
/*
FATP_META:
  meta_version: 1
  component: ContractException
  file_role: test
  path: components/ContractException/tests/test_ContractException.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for ContractException."
  api_stability: in_work
  related:
    docs_search: "ContractException"
    headers:
      - include/fat_p/ContractException.h
      - include/fat_p/FatPTest.h
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
#include <climits>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeinfo>
#include <vector>

#include "ContractException.h"
#include "FatPTest.h"

namespace fat_p::testing::contractexception
{

using namespace fat_p;

// =============================================================================
// Test Suite 1: Basic Functionality
// =============================================================================

FATP_TEST_CASE(logic_contract_error_basic)
{
    try
    {
        throw LogicContractError("Test logic error");
        FATP_ASSERT_TRUE(false, "Should have thrown LogicContractError");
    }
    catch (const LogicContractError& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("Contract Violation:") != std::string::npos,
                         "Message contains 'Contract Violation:' prefix");
        FATP_ASSERT_TRUE(msg.find("Test logic error") != std::string::npos, "Message contains user text");
    }
    return true;
}

FATP_TEST_CASE(runtime_contract_error_basic)
{
    try
    {
        throw RuntimeContractError("Test runtime error");
        FATP_ASSERT_TRUE(false, "Should have thrown RuntimeContractError");
    }
    catch (const RuntimeContractError& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("Contract Violation:") != std::string::npos,
                         "Message contains 'Contract Violation:' prefix");
        FATP_ASSERT_TRUE(msg.find("Test runtime error") != std::string::npos, "Message contains user text");
    }
    return true;
}

FATP_TEST_CASE(alloc_contract_error_basic)
{
    try
    {
        throw AllocContractError("Stack overflow detected");
        FATP_ASSERT_TRUE(false, "Should have thrown AllocContractError");
    }
    catch (const AllocContractError& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("Contract Violation:") != std::string::npos,
                         "Message contains 'Contract Violation:' prefix");
        FATP_ASSERT_TRUE(msg.find("Bad Allocation") != std::string::npos, "Message contains 'Bad Allocation'");
        FATP_ASSERT_TRUE(msg.find("Stack overflow detected") != std::string::npos, "Message contains user text");

        // Verify no double prefix
        std::string::size_type first_pos = msg.find("Contract Violation:");
        std::string::size_type second_pos = msg.find("Contract Violation:", first_pos + 1);
        FATP_ASSERT_TRUE(second_pos == std::string::npos,
                         "Message should NOT contain duplicate 'Contract Violation:' prefix");
    }
    return true;
}

FATP_TEST_CASE(domain_contract_error)
{
    try
    {
        throw DomainContractError("sqrt of negative number");
        FATP_ASSERT_TRUE(false, "Should have thrown DomainContractError");
    }
    catch (const DomainContractError& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("sqrt of negative number") != std::string::npos, "Message contains user text");
    }
    catch (const std::domain_error&)
    {
        // Also acceptable (inheritance check)
    }
    return true;
}

FATP_TEST_CASE(out_of_range_contract_error)
{
    try
    {
        throw OutOfRangeContractError("Index 10 out of bounds [0, 5)");
        FATP_ASSERT_TRUE(false, "Should have thrown OutOfRangeContractError");
    }
    catch (const OutOfRangeContractError& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("Index 10") != std::string::npos, "Message contains index info");
    }
    catch (const std::out_of_range&)
    {
        // Also acceptable (inheritance check)
    }
    return true;
}

FATP_TEST_CASE(overflow_underflow_contract_errors)
{
    // Test OverflowContractError
    try
    {
        throw OverflowContractError("Integer overflow: INT_MAX + 1");
        FATP_ASSERT_TRUE(false, "Should have thrown OverflowContractError");
    }
    catch (const OverflowContractError& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("overflow") != std::string::npos, "Message contains 'overflow'");
    }

    // Test UnderflowContractError
    try
    {
        throw UnderflowContractError("Integer underflow: INT_MIN - 1");
        FATP_ASSERT_TRUE(false, "Should have thrown UnderflowContractError");
    }
    catch (const UnderflowContractError& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("underflow") != std::string::npos, "Message contains 'underflow'");
    }

    return true;
}

FATP_TEST_CASE(invalid_argument_contract_error)
{
    try
    {
        throw InvalidArgumentContractError("Null pointer argument");
        FATP_ASSERT_TRUE(false, "Should have thrown InvalidArgumentContractError");
    }
    catch (const InvalidArgumentContractError& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("Null pointer") != std::string::npos, "Message contains error details");
    }
    catch (const std::invalid_argument&)
    {
        // Also acceptable (inheritance check)
    }
    return true;
}

// =============================================================================
// Test Suite 2: Polymorphic Base Class
// =============================================================================

FATP_TEST_CASE(polymorphic_catch_base)
{
    bool caught_as_base = false;

    try
    {
        throw LogicContractError("Logic error for polymorphic test");
    }
    catch (const ContractViolationBase& e)
    {
        caught_as_base = true;
        FATP_ASSERT_TRUE(std::string(e.message()).find("Logic error") != std::string::npos,
                         "Polymorphic catch preserves message");
    }

    FATP_ASSERT_TRUE(caught_as_base, "Should have caught as ContractViolationBase");
    return true;
}

FATP_TEST_CASE(polymorphic_catch_all_types)
{
    std::vector<std::string> categories_caught;

    try
    {
        throw LogicContractError("test");
    }
    catch (const ContractViolationBase& e)
    {
        categories_caught.push_back(e.category());
    }

    try
    {
        throw RuntimeContractError("test");
    }
    catch (const ContractViolationBase& e)
    {
        categories_caught.push_back(e.category());
    }

    try
    {
        throw AllocContractError("test");
    }
    catch (const ContractViolationBase& e)
    {
        categories_caught.push_back(e.category());
    }

    try
    {
        throw DomainContractError("test");
    }
    catch (const ContractViolationBase& e)
    {
        categories_caught.push_back(e.category());
    }

    FATP_ASSERT_TRUE(categories_caught.size() == 4, "Should have caught all 4 exception types polymorphically");

    return true;
}

FATP_TEST_CASE(category_method)
{
    // Test LogicContractError category
    try
    {
        throw LogicContractError("test");
    }
    catch (const ContractViolationBase& e)
    {
        FATP_ASSERT_TRUE(std::string(e.category()) == "Logic", "LogicContractError should return 'Logic' category");
    }

    // Test RuntimeContractError category
    try
    {
        throw RuntimeContractError("test");
    }
    catch (const ContractViolationBase& e)
    {
        FATP_ASSERT_TRUE(std::string(e.category()) == "Runtime",
                         "RuntimeContractError should return 'Runtime' category");
    }

    // Test AllocContractError category
    try
    {
        throw AllocContractError("test");
    }
    catch (const ContractViolationBase& e)
    {
        FATP_ASSERT_TRUE(std::string(e.category()) == "Allocation",
                         "AllocContractError should return 'Allocation' category");
    }

    return true;
}

// =============================================================================
// Test Suite 3: Standard Exception Compatibility
// =============================================================================

FATP_TEST_CASE(logic_error_inheritance)
{
    bool caught_as_logic_error = false;

    try
    {
        throw LogicContractError("test");
    }
    catch (const std::logic_error& e)
    {
        caught_as_logic_error = true;
        FATP_ASSERT_TRUE(std::string(e.what()).find("test") != std::string::npos,
                         "Message preserved when caught as std::logic_error");
    }

    FATP_ASSERT_TRUE(caught_as_logic_error, "LogicContractError should be catchable as std::logic_error");
    return true;
}

FATP_TEST_CASE(runtime_error_inheritance)
{
    bool caught_as_runtime_error = false;

    try
    {
        throw RuntimeContractError("test");
    }
    catch (const std::runtime_error& e)
    {
        caught_as_runtime_error = true;
        FATP_ASSERT_TRUE(std::string(e.what()).find("test") != std::string::npos,
                         "Message preserved when caught as std::runtime_error");
    }

    FATP_ASSERT_TRUE(caught_as_runtime_error, "RuntimeContractError should be catchable as std::runtime_error");
    return true;
}

FATP_TEST_CASE(bad_alloc_inheritance)
{
    bool caught_as_bad_alloc = false;

    try
    {
        throw AllocContractError("Custom allocation failure");
    }
    catch (const std::bad_alloc& e)
    {
        caught_as_bad_alloc = true;
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("allocation") != std::string::npos || msg.find("Allocation") != std::string::npos,
                         "Message should mention allocation when caught as std::bad_alloc");
    }

    FATP_ASSERT_TRUE(caught_as_bad_alloc, "AllocContractError should be catchable as std::bad_alloc");
    return true;
}

FATP_TEST_CASE(exception_hierarchy)
{
    int exception_count = 0;

    try
    {
        throw LogicContractError("test1");
    }
    catch (const std::exception&)
    {
        ++exception_count;
    }

    try
    {
        throw RuntimeContractError("test2");
    }
    catch (const std::exception&)
    {
        ++exception_count;
    }

    try
    {
        throw AllocContractError("test3");
    }
    catch (const std::exception&)
    {
        ++exception_count;
    }

    FATP_ASSERT_TRUE(exception_count == 3, "All contract exceptions should be catchable as std::exception");
    return true;
}

// =============================================================================
// Test Suite 4: Message Formatting
// =============================================================================

FATP_TEST_CASE(message_with_variables)
{
    int value = 42;
    std::string var_name = "x";

    try
    {
        std::ostringstream oss;
        oss << "Invalid value for " << var_name << ": " << value;
        throw LogicContractError(oss.str());
    }
    catch (const LogicContractError& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("42") != std::string::npos, "Message contains interpolated value");
        FATP_ASSERT_TRUE(msg.find("x") != std::string::npos, "Message contains variable name");
    }

    return true;
}

FATP_TEST_CASE(message_with_special_characters)
{
    try
    {
        throw RuntimeContractError("Path: /tmp/file\nLine: 10\tColumn: 5");
    }
    catch (const RuntimeContractError& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("/tmp/file") != std::string::npos, "Message handles path with slash");
        FATP_ASSERT_TRUE(msg.find("Line:") != std::string::npos, "Message handles newline in string");
        FATP_ASSERT_TRUE(msg.find("Column:") != std::string::npos, "Message handles tab in string");
    }

    return true;
}

FATP_TEST_CASE(long_message)
{
    std::string long_msg(1000, 'x');
    try
    {
        throw LogicContractError(long_msg);
    }
    catch (const LogicContractError& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.length() > 1000, "Long message preserved (with prefix)");
        FATP_ASSERT_TRUE(msg.find('x') != std::string::npos, "Long message content preserved");
    }

    return true;
}

FATP_TEST_CASE(empty_message)
{
    try
    {
        throw LogicContractError("");
    }
    catch (const LogicContractError& e)
    {
        std::string msg = e.what();
        FATP_ASSERT_TRUE(msg.find("Contract Violation:") != std::string::npos, "Empty message still gets prefix");
    }

    return true;
}

// =============================================================================
// Test Suite 5: Stream Operator
// =============================================================================

FATP_TEST_CASE(stream_operator_logic)
{
    LogicContractError e("precondition failed");
    std::ostringstream oss;

    oss << static_cast<const ContractViolationBase&>(e);
    std::string output = oss.str();

    FATP_ASSERT_TRUE(output.find("[Logic]") != std::string::npos, "Stream output contains category in brackets");
    FATP_ASSERT_TRUE(output.find("Contract Violation:") != std::string::npos, "Stream output contains message");
    FATP_ASSERT_TRUE(output.find("precondition failed") != std::string::npos, "Stream output contains user text");

    return true;
}

FATP_TEST_CASE(stream_operator_runtime)
{
    RuntimeContractError e("connection timeout");
    std::ostringstream oss;

    oss << static_cast<const ContractViolationBase&>(e);
    std::string output = oss.str();

    FATP_ASSERT_TRUE(output.find("[Runtime]") != std::string::npos, "Stream output contains Runtime category");
    FATP_ASSERT_TRUE(output.find("connection timeout") != std::string::npos, "Stream output contains user text");

    return true;
}

FATP_TEST_CASE(stream_operator_alloc)
{
    AllocContractError e("pool exhausted");
    std::ostringstream oss;

    oss << static_cast<const ContractViolationBase&>(e);
    std::string output = oss.str();

    FATP_ASSERT_TRUE(output.find("[Allocation]") != std::string::npos, "Stream output contains Allocation category");
    FATP_ASSERT_TRUE(output.find("pool exhausted") != std::string::npos, "Stream output contains user text");

    return true;
}

FATP_TEST_CASE(stream_operator_all_types)
{
    std::ostringstream oss;

    LogicContractError e1("test");
    RuntimeContractError e2("test");
    DomainContractError e3("test");
    OutOfRangeContractError e4("test");
    InvalidArgumentContractError e5("test");
    OverflowContractError e6("test");
    UnderflowContractError e7("test");
    AllocContractError e8("test");

    oss << static_cast<const ContractViolationBase&>(e1) << "\n";
    oss << static_cast<const ContractViolationBase&>(e2) << "\n";
    oss << static_cast<const ContractViolationBase&>(e3) << "\n";
    oss << static_cast<const ContractViolationBase&>(e4) << "\n";
    oss << static_cast<const ContractViolationBase&>(e5) << "\n";
    oss << static_cast<const ContractViolationBase&>(e6) << "\n";
    oss << static_cast<const ContractViolationBase&>(e7) << "\n";
    oss << static_cast<const ContractViolationBase&>(e8) << "\n";

    std::string output = oss.str();

    FATP_ASSERT_TRUE(output.find("[Logic]") != std::string::npos, "Output contains Logic category");
    FATP_ASSERT_TRUE(output.find("[Runtime]") != std::string::npos, "Output contains Runtime category");
    FATP_ASSERT_TRUE(output.find("[Allocation]") != std::string::npos, "Output contains Allocation category");

    return true;
}

// =============================================================================
// Test Suite 6: Move Semantics and noexcept
// =============================================================================

FATP_TEST_CASE(move_constructor_logic)
{
    LogicContractError e1("movable message");
    LogicContractError e2 = std::move(e1);

    std::string msg = e2.what();
    FATP_ASSERT_TRUE(msg.find("movable message") != std::string::npos, "Move constructor preserves message");

    return true;
}

FATP_TEST_CASE(move_constructor_alloc)
{
    AllocContractError e1("alloc movable");
    AllocContractError e2 = std::move(e1);

    std::string msg = e2.what();
    FATP_ASSERT_TRUE(msg.find("alloc movable") != std::string::npos,
                     "AllocContractError move constructor preserves message");

    return true;
}

FATP_TEST_CASE(move_noexcept_alloc)
{
    FATP_ASSERT_TRUE(std::is_nothrow_move_constructible_v<AllocContractError>,
                     "AllocContractError should be nothrow move constructible");
    FATP_ASSERT_TRUE(std::is_nothrow_move_assignable_v<AllocContractError>,
                     "AllocContractError should be nothrow move assignable");

    return true;
}

FATP_TEST_CASE(copy_semantics)
{
    LogicContractError e1("original");
    LogicContractError e2 = e1;

    FATP_ASSERT_TRUE(std::string(e1.what()) == std::string(e2.what()), "Copy constructor produces identical message");

    AllocContractError a1("alloc original");
    AllocContractError a2 = a1;

    FATP_ASSERT_TRUE(std::string(a1.what()) == std::string(a2.what()),
                     "AllocContractError copy produces identical message");

    return true;
}

FATP_TEST_CASE(noexcept_methods)
{
    LogicContractError e("test");
    AllocContractError ae("test");

    // Note: std::logic_error::what() noexcept specification varies by stdlib implementation.
    // We only test our own methods which are explicitly marked noexcept.
    FATP_ASSERT_TRUE(noexcept(e.category()), "category() should be noexcept");
    FATP_ASSERT_TRUE(noexcept(e.message()), "message() should be noexcept");
    FATP_ASSERT_TRUE(noexcept(ae.what()), "AllocContractError::what() should be noexcept");
    FATP_ASSERT_TRUE(noexcept(ae.category()), "AllocContractError::category() should be noexcept");
    FATP_ASSERT_TRUE(noexcept(ae.message()), "AllocContractError::message() should be noexcept");

    return true;
}

FATP_TEST_CASE(rethrow_preserves_message)
{
    try
    {
        try
        {
            throw RuntimeContractError("inner throw message");
        }
        catch (...)
        {
            throw;
        }
    }
    catch (const ContractViolationBase& e)
    {
        FATP_ASSERT_TRUE(std::string(e.message()).find("inner throw message") != std::string::npos,
                         "Message preserved after rethrow");
    }

    return true;
}

// =============================================================================
// Test Suite 7: Thread Safety
// =============================================================================

FATP_TEST_CASE(concurrent_exception_throwing)
{
    const int num_threads = 4;
    const int iterations = 1000;
    std::atomic<int> logic_count{0};
    std::atomic<int> runtime_count{0};
    std::atomic<int> alloc_count{0};

    auto thread_func = [&](int thread_id) {
        for (int i = 0; i < iterations; ++i)
        {
            try
            {
                switch (i % 3)
                {
                    case 0:
                        throw LogicContractError("Thread " + std::to_string(thread_id));
                    case 1:
                        throw RuntimeContractError("Thread " + std::to_string(thread_id));
                    case 2:
                        throw AllocContractError("Thread " + std::to_string(thread_id));
                }
            }
            catch (const ContractViolationBase& e)
            {
                std::string cat = e.category();
                if (cat == "Logic")
                {
                    ++logic_count;
                }
                else if (cat == "Runtime")
                {
                    ++runtime_count;
                }
                else if (cat == "Allocation")
                {
                    ++alloc_count;
                }
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(thread_func, i);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    int base = iterations / 3;
    int rem = iterations % 3;
    int expected_logic = num_threads * (base + (rem > 0 ? 1 : 0));
    int expected_runtime = num_threads * (base + (rem > 1 ? 1 : 0));
    int expected_alloc = num_threads * base;

    FATP_ASSERT_TRUE(logic_count.load() == expected_logic, "Logic exceptions counted correctly in concurrent test");
    FATP_ASSERT_TRUE(runtime_count.load() == expected_runtime,
                     "Runtime exceptions counted correctly in concurrent test");
    FATP_ASSERT_TRUE(alloc_count.load() == expected_alloc, "Alloc exceptions counted correctly in concurrent test");

    return true;
}

// =============================================================================
// Test Suite 8: Integration Scenarios
// =============================================================================

FATP_TEST_CASE(factory_error_pattern)
{
    auto create_object = [](const std::string& key) {
        if (key.empty())
        {
            throw InvalidArgumentContractError("Factory key cannot be empty");
        }
        if (key == "invalid")
        {
            throw RuntimeContractError("Factory: Unknown key '" + key + "'");
        }
        return 42;
    };

    try
    {
        create_object("");
        FATP_ASSERT_TRUE(false, "Should have thrown InvalidArgumentContractError");
    }
    catch (const InvalidArgumentContractError& e)
    {
        FATP_ASSERT_TRUE(std::string(e.what()).find("empty") != std::string::npos,
                         "Factory error message is descriptive");
    }

    try
    {
        create_object("invalid");
        FATP_ASSERT_TRUE(false, "Should have thrown RuntimeContractError");
    }
    catch (const RuntimeContractError& e)
    {
        FATP_ASSERT_TRUE(std::string(e.what()).find("Unknown key") != std::string::npos,
                         "Factory error message contains key info");
    }

    return true;
}

FATP_TEST_CASE(checked_arithmetic_pattern)
{
    auto checked_add = [](int a, int b) -> int {
        if (a > 0 && b > INT_MAX - a)
        {
            throw OverflowContractError("Integer overflow in addition");
        }
        return a + b;
    };

    try
    {
        checked_add(INT_MAX, 1);
        FATP_ASSERT_TRUE(false, "Should have thrown OverflowContractError");
    }
    catch (const OverflowContractError& e)
    {
        FATP_ASSERT_TRUE(std::string(e.category()) == "Runtime", "Overflow error has Runtime category");
    }

    return true;
}

FATP_TEST_CASE(allocator_pattern)
{
    auto allocate = [](size_t size) -> void* {
        constexpr size_t MAX_SIZE = 1024 * 1024;
        if (size > MAX_SIZE)
        {
            throw AllocContractError("Allocation size " + std::to_string(size) + " exceeds limit " +
                                     std::to_string(MAX_SIZE));
        }
        if (size > 0)
        {
            throw AllocContractError("Out of memory");
        }
        return nullptr;
    };

    try
    {
        allocate(2 * 1024 * 1024);
        FATP_ASSERT_TRUE(false, "Should have thrown AllocContractError for size limit");
    }
    catch (const std::bad_alloc& e)
    {
        FATP_ASSERT_TRUE(std::string(e.what()).find("exceeds limit") != std::string::npos,
                         "Allocation error message is descriptive");
    }

    try
    {
        allocate(100);
        FATP_ASSERT_TRUE(false, "Should have thrown AllocContractError for OOM");
    }
    catch (const AllocContractError& e)
    {
        FATP_ASSERT_TRUE(std::string(e.what()).find("Out of memory") != std::string::npos,
                         "OOM error message is clear");
        FATP_ASSERT_TRUE(std::string(e.category()) == "Allocation", "AllocContractError has Allocation category");
    }

    return true;
}

FATP_TEST_CASE(nested_exception_pattern)
{
    try
    {
        try
        {
            throw LogicContractError("outer");
        }
        catch (...)
        {
            std::throw_with_nested(RuntimeContractError("inner"));
        }
    }
    catch (const std::exception& e)
    {
        FATP_ASSERT_TRUE(std::string(e.what()).find("inner") != std::string::npos,
                         "Outer exception contains inner message");

        try
        {
            std::rethrow_if_nested(e);
            FATP_ASSERT_TRUE(false, "Should have rethrown nested exception");
        }
        catch (const ContractViolationBase& nested)
        {
            FATP_ASSERT_TRUE(std::string(nested.message()).find("outer") != std::string::npos,
                             "Nested exception preserved");
        }
    }

    return true;
}

// =============================================================================
// Benchmarks
// =============================================================================

void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}


} // namespace fat_p::testing::contractexception

namespace fat_p::testing
{

// =============================================================================
// Test Runner
// =============================================================================

bool test_ContractException()
{
    FATP_PRINT_HEADER(CONTRACT EXCEPTION)

    TestRunner runner;

    // Test Suite 1: Basic Functionality
    std::cout << colors::cyan() << "Test Suite 1: Basic Functionality" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, contractexception, logic_contract_error_basic);
    FATP_RUN_TEST_NS(runner, contractexception, runtime_contract_error_basic);
    FATP_RUN_TEST_NS(runner, contractexception, alloc_contract_error_basic);
    FATP_RUN_TEST_NS(runner, contractexception, domain_contract_error);
    FATP_RUN_TEST_NS(runner, contractexception, out_of_range_contract_error);
    FATP_RUN_TEST_NS(runner, contractexception, overflow_underflow_contract_errors);
    FATP_RUN_TEST_NS(runner, contractexception, invalid_argument_contract_error);

    // Test Suite 2: Polymorphic Base Class
    std::cout << "\n" << colors::cyan() << "Test Suite 2: Polymorphic Base Class" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, contractexception, polymorphic_catch_base);
    FATP_RUN_TEST_NS(runner, contractexception, polymorphic_catch_all_types);
    FATP_RUN_TEST_NS(runner, contractexception, category_method);

    // Test Suite 3: Standard Exception Compatibility
    std::cout << "\n" << colors::cyan() << "Test Suite 3: Standard Exception Compatibility" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, contractexception, logic_error_inheritance);
    FATP_RUN_TEST_NS(runner, contractexception, runtime_error_inheritance);
    FATP_RUN_TEST_NS(runner, contractexception, bad_alloc_inheritance);
    FATP_RUN_TEST_NS(runner, contractexception, exception_hierarchy);

    // Test Suite 4: Message Formatting
    std::cout << "\n" << colors::cyan() << "Test Suite 4: Message Formatting" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, contractexception, message_with_variables);
    FATP_RUN_TEST_NS(runner, contractexception, message_with_special_characters);
    FATP_RUN_TEST_NS(runner, contractexception, long_message);
    FATP_RUN_TEST_NS(runner, contractexception, empty_message);

    // Test Suite 5: Stream Operator
    std::cout << "\n" << colors::cyan() << "Test Suite 5: Stream Operator" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, contractexception, stream_operator_logic);
    FATP_RUN_TEST_NS(runner, contractexception, stream_operator_runtime);
    FATP_RUN_TEST_NS(runner, contractexception, stream_operator_alloc);
    FATP_RUN_TEST_NS(runner, contractexception, stream_operator_all_types);

    // Test Suite 6: Move Semantics and noexcept
    std::cout << "\n" << colors::cyan() << "Test Suite 6: Move Semantics and noexcept" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, contractexception, move_constructor_logic);
    FATP_RUN_TEST_NS(runner, contractexception, move_constructor_alloc);
    FATP_RUN_TEST_NS(runner, contractexception, move_noexcept_alloc);
    FATP_RUN_TEST_NS(runner, contractexception, copy_semantics);
    FATP_RUN_TEST_NS(runner, contractexception, noexcept_methods);
    FATP_RUN_TEST_NS(runner, contractexception, rethrow_preserves_message);

    // Test Suite 7: Thread Safety
    std::cout << "\n" << colors::cyan() << "Test Suite 7: Thread Safety" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, contractexception, concurrent_exception_throwing);

    // Test Suite 8: Integration Scenarios
    std::cout << "\n" << colors::cyan() << "Test Suite 8: Integration Scenarios" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, contractexception, factory_error_pattern);
    FATP_RUN_TEST_NS(runner, contractexception, checked_arithmetic_pattern);
    FATP_RUN_TEST_NS(runner, contractexception, allocator_pattern);
    FATP_RUN_TEST_NS(runner, contractexception, nested_exception_pattern);

    contractexception::run_benchmarks();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_ContractException() ? 0 : 1;
}
#endif
