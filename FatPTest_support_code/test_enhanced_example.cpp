#include "test_Utilities_enhanced.h"
#include <vector>
#include <string>

using namespace fat_p::testing;

struct DatabaseFixture : public TestFixture
{
    int* connection;
    
    void SetUp() override
    {
        connection = new int(42);
    }
    
    void TearDown() override
    {
        delete connection;
        connection = nullptr;
    }
};

bool test_string_assertions()
{
    ASSERT_CONTAINS("hello world", "world", "Contains test");
    ASSERT_NOT_CONTAINS("hello world", "xyz", "Not contains test");
    ASSERT_STARTS_WITH("hello world", "hello", "Starts with test");
    ASSERT_ENDS_WITH("hello world", "world", "Ends with test");
    ASSERT_MATCHES("test123", "test[0-9]+", "Regex match test");
    ASSERT_STR_EQ_IGNORE_CASE("Hello", "hello", "Case insensitive test");
    return true;
}

bool test_container_assertions()
{
    std::vector<int> v1 = {1, 2, 3, 4, 5};
    std::vector<int> v2 = {1, 2, 3, 4, 5};
    ASSERT_RANGE_EQ(v1, v2, "Vectors equal");
    
    std::vector<double> d1 = {1.0, 2.0, 3.0};
    std::vector<double> d2 = {1.0001, 2.0001, 3.0001};
    ASSERT_RANGE_CLOSE(d1, d2, 0.001, "Vectors close");
    
    return true;
}

bool test_with_fixture(DatabaseFixture& fixture)
{
    ASSERT_NOT_NULLPTR(fixture.connection, "Connection initialized");
    ASSERT_EQ(*fixture.connection, 42, "Connection value correct");
    return true;
}

struct AdditionTestCase
{
    int a;
    int b;
    int expected;
    std::string description;
};

bool test_parameterized()
{
    std::vector<AdditionTestCase> cases = {
        {2, 3, 5, "2+3=5"},
        {10, 20, 30, "10+20=30"},
        {-1, 1, 0, "-1+1=0"},
        {0, 0, 0, "0+0=0"}
    };
    
    return run_parameterized_test("addition", cases, [](const AdditionTestCase& tc) {
        int result = tc.a + tc.b;
        ASSERT_EQ(result, tc.expected, tc.description.c_str());
        return true;
    });
}

int main()
{
    TestRunner runner;
    
    PRINT_HEADER(ENHANCED FEATURES TEST);
    
    runner.run_test("string_assertions", test_string_assertions);
    runner.run_test("container_assertions", test_container_assertions);
    runner.run_test_with_fixture<DatabaseFixture>("fixture_test", test_with_fixture);
    runner.run_test("parameterized_test", test_parameterized);
    
    auto simple_func = []() { int x = 0; for (int i = 0; i < 100; ++i) { x += i; } DoNotOptimize(x); };
    benchmark("simple_loop", simple_func, 10000);
    
    benchmark_detailed("enhanced_benchmark", simple_func, 10000, 20, true);
    
    runner.set_filter("*assertions");
    
    return runner.print_summary();
}
