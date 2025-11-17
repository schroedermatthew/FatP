#include <iostream>
#include <vector>
#include <string>

#include "BinarySerializer.h"
#include "Expected.h"
#include "test_BinarySerializer.h"
#include "FatPTest.h"

namespace fat_p::testing
{

// =============================================================================
// CustomBinaryPolicy Tests
// =============================================================================

bool test_custom_uint64() {
    BinarySerializer<CustomBinaryPolicy> serializer;
    
    uint64_t original = 9876543210ULL;
    auto serialized = serializer.serialize(original);
    SIMPLE_ASSERT(serialized.has_value(), "Serialize uint64_t should succeed");
    SIMPLE_ASSERT(!serialized->empty(), "Serialized data should not be empty");
    
    auto deserialized = serializer.deserialize_uint64(*serialized);
    SIMPLE_ASSERT(deserialized.has_value(), "Deserialize uint64_t should succeed");
    SIMPLE_ASSERT(*deserialized == original, "Round trip should preserve uint64_t value");
    
    return true;
}

bool test_custom_int64() {
    BinarySerializer<CustomBinaryPolicy> serializer;
    
    int64_t original = -9876543210LL;
    auto serialized = serializer.serialize(original);
    SIMPLE_ASSERT(serialized.has_value(), "Serialize int64_t should succeed");
    
    auto deserialized = serializer.deserialize_int64(*serialized);
    SIMPLE_ASSERT(deserialized.has_value(), "Deserialize int64_t should succeed");
    SIMPLE_ASSERT(*deserialized == original, "Round trip should preserve int64_t value");
    
    return true;
}

bool test_custom_bool() {
    BinarySerializer<CustomBinaryPolicy> serializer;
    
    bool original_true = true;
    auto serialized_true = serializer.serialize(original_true);
    SIMPLE_ASSERT(serialized_true.has_value(), "Serialize bool (true) should succeed");
    
    auto deserialized_true = serializer.deserialize_bool(*serialized_true);
    SIMPLE_ASSERT(deserialized_true.has_value(), "Deserialize bool (true) should succeed");
    SIMPLE_ASSERT(*deserialized_true == original_true, "Round trip should preserve bool (true) value");
    
    bool original_false = false;
    auto serialized_false = serializer.serialize(original_false);
    SIMPLE_ASSERT(serialized_false.has_value(), "Serialize bool (false) should succeed");
    
    auto deserialized_false = serializer.deserialize_bool(*serialized_false);
    SIMPLE_ASSERT(deserialized_false.has_value(), "Deserialize bool (false) should succeed");
    SIMPLE_ASSERT(*deserialized_false == original_false, "Round trip should preserve bool (false) value");
    
    return true;
}

bool test_custom_double() {
    BinarySerializer<CustomBinaryPolicy> serializer;
    
    double original = 3.14159265358979;
    auto serialized = serializer.serialize(original);
    SIMPLE_ASSERT(serialized.has_value(), "Serialize double should succeed");
    
    auto deserialized = serializer.deserialize_double(*serialized);
    SIMPLE_ASSERT(deserialized.has_value(), "Deserialize double should succeed");
    SIMPLE_ASSERT(*deserialized == original, "Round trip should preserve double value");
    
    return true;
}

bool test_custom_string() {
    BinarySerializer<CustomBinaryPolicy> serializer;
    
    std::string original = "Hello, BinarySerializer!";
    auto serialized = serializer.serialize(original);
    SIMPLE_ASSERT(serialized.has_value(), "Serialize string should succeed");
    
    auto deserialized = serializer.deserialize_string(*serialized);
    SIMPLE_ASSERT(deserialized.has_value(), "Deserialize string should succeed");
    SIMPLE_ASSERT(*deserialized == original, "Round trip should preserve string value");
    
    return true;
}

bool test_custom_empty_string() {
    BinarySerializer<CustomBinaryPolicy> serializer;
    
    std::string original = "";
    auto serialized = serializer.serialize(original);
    SIMPLE_ASSERT(serialized.has_value(), "Serialize empty string should succeed");
    
    auto deserialized = serializer.deserialize_string(*serialized);
    SIMPLE_ASSERT(deserialized.has_value(), "Deserialize empty string should succeed");
    SIMPLE_ASSERT(*deserialized == original, "Round trip should preserve empty string");
    
    return true;
}

bool test_custom_vector_int() {
    BinarySerializer<CustomBinaryPolicy> serializer;
    
    std::vector<int64_t> original = {1, 2, 3, 4, 5};
    auto serialized = serializer.serialize(original);
    SIMPLE_ASSERT(serialized.has_value(), "Serialize vector<int64_t> should succeed");
    
    auto deserialized = serializer.deserialize_vector<int64_t>(*serialized);
    SIMPLE_ASSERT(deserialized.has_value(), "Deserialize vector<int64_t> should succeed");
    SIMPLE_ASSERT(deserialized->size() == original.size(), "Vector size should match");
    SIMPLE_ASSERT(*deserialized == original, "Round trip should preserve vector<int64_t> value");
    
    return true;
}

bool test_custom_vector_string() {
    BinarySerializer<CustomBinaryPolicy> serializer;
    
    std::vector<std::string> original = {"one", "two", "three"};
    auto serialized = serializer.serialize(original);
    SIMPLE_ASSERT(serialized.has_value(), "Serialize vector<string> should succeed");
    
    auto deserialized = serializer.deserialize_vector<std::string>(*serialized);
    SIMPLE_ASSERT(deserialized.has_value(), "Deserialize vector<string> should succeed");
    SIMPLE_ASSERT(deserialized->size() == original.size(), "Vector size should match");
    SIMPLE_ASSERT(*deserialized == original, "Round trip should preserve vector<string> value");
    
    return true;
}

bool test_custom_empty_vector() {
    BinarySerializer<CustomBinaryPolicy> serializer;
    
    std::vector<uint64_t> original;
    auto serialized = serializer.serialize(original);
    SIMPLE_ASSERT(serialized.has_value(), "Serialize empty vector should succeed");
    
    auto deserialized = serializer.deserialize_vector<uint64_t>(*serialized);
    SIMPLE_ASSERT(deserialized.has_value(), "Deserialize empty vector should succeed");
    SIMPLE_ASSERT(deserialized->empty(), "Deserialized vector should be empty");
    
    return true;
}

// =============================================================================
// CborPolicy Tests
// =============================================================================

bool test_cbor_uint64() {
    BinarySerializer<CborPolicy> serializer;
    
    uint64_t original = 9876543210ULL;
    auto serialized = serializer.serialize(original);
    SIMPLE_ASSERT(serialized.has_value(), "CBOR serialize uint64_t should succeed");
    SIMPLE_ASSERT(!serialized->empty(), "CBOR serialized data should not be empty");
    
    auto deserialized = serializer.deserialize_uint64(*serialized);
    SIMPLE_ASSERT(deserialized.has_value(), "CBOR deserialize uint64_t should succeed");
    SIMPLE_ASSERT(*deserialized == original, "CBOR round trip should preserve uint64_t value");
    
    return true;
}

bool test_cbor_int64_negative() {
    BinarySerializer<CborPolicy> serializer;
    
    int64_t original = -123456;
    auto serialized = serializer.serialize(original);
    SIMPLE_ASSERT(serialized.has_value(), "CBOR serialize negative int64_t should succeed");
    
    auto deserialized = serializer.deserialize_int64(*serialized);
    SIMPLE_ASSERT(deserialized.has_value(), "CBOR deserialize negative int64_t should succeed");
    SIMPLE_ASSERT(*deserialized == original, "CBOR round trip should preserve negative int64_t value");
    
    return true;
}

bool test_cbor_bool() {
    BinarySerializer<CborPolicy> serializer;
    
    bool original_true = true;
    auto serialized_true = serializer.serialize(original_true);
    SIMPLE_ASSERT(serialized_true.has_value(), "CBOR serialize bool (true) should succeed");
    
    auto deserialized_true = serializer.deserialize_bool(*serialized_true);
    SIMPLE_ASSERT(deserialized_true.has_value(), "CBOR deserialize bool (true) should succeed");
    SIMPLE_ASSERT(*deserialized_true == original_true, "CBOR round trip should preserve bool (true) value");
    
    return true;
}

bool test_cbor_double() {
    BinarySerializer<CborPolicy> serializer;
    
    double original = 3.14159265358979;
    auto serialized = serializer.serialize(original);
    SIMPLE_ASSERT(serialized.has_value(), "CBOR serialize double should succeed");
    
    auto deserialized = serializer.deserialize_double(*serialized);
    SIMPLE_ASSERT(deserialized.has_value(), "CBOR deserialize double should succeed");
    SIMPLE_ASSERT(*deserialized == original, "CBOR round trip should preserve double value");
    
    return true;
}

bool test_cbor_string() {
    BinarySerializer<CborPolicy> serializer;
    
    std::string original = "Hello, CBOR!";
    auto serialized = serializer.serialize(original);
    SIMPLE_ASSERT(serialized.has_value(), "CBOR serialize string should succeed");
    
    auto deserialized = serializer.deserialize_string(*serialized);
    SIMPLE_ASSERT(deserialized.has_value(), "CBOR deserialize string should succeed");
    SIMPLE_ASSERT(*deserialized == original, "CBOR round trip should preserve string value");
    
    return true;
}

bool test_cbor_vector_int() {
    BinarySerializer<CborPolicy> serializer;
    
    std::vector<int64_t> original = {1, -2, 3, -4, 5};
    auto serialized = serializer.serialize(original);
    SIMPLE_ASSERT(serialized.has_value(), "CBOR serialize vector<int64_t> should succeed");
    
    auto deserialized = serializer.deserialize_vector<int64_t>(*serialized);
    SIMPLE_ASSERT(deserialized.has_value(), "CBOR deserialize vector<int64_t> should succeed");
    SIMPLE_ASSERT(deserialized->size() == original.size(), "CBOR vector size should match");
    SIMPLE_ASSERT(*deserialized == original, "CBOR round trip should preserve vector<int64_t> value");
    
    return true;
}

bool test_cbor_compact_encoding() {
    BinarySerializer<CborPolicy> serializer;
    
    // Small numbers should use compact encoding
    uint64_t small = 10;
    auto small_buf = serializer.serialize(small);
    SIMPLE_ASSERT(small_buf.has_value(), "CBOR small number serialize should succeed");
    SIMPLE_ASSERT(small_buf->size() == 1, "CBOR small number should be 1 byte");
    
    return true;
}

// =============================================================================
// Benchmarks
// =============================================================================

void benchmark_binaryserializer() {
    std::cout << "\n" << colors::cyan() << "BinarySerializer Benchmarks:" << colors::reset() << "\n\n";
    
    // Custom Binary Policy Benchmarks
    std::cout << colors::yellow() << "CustomBinaryPolicy:" << colors::reset() << "\n";
    {
        BinarySerializer<CustomBinaryPolicy> serializer;
        
        double serialize_time = measure_perf([&serializer, i=0]() mutable {
            auto result = serializer.serialize(static_cast<uint64_t>(i));
            DoNotOptimize(result);
            ++i;
        }, 100000, 1000);
        std::cout << "  Serialize uint64_t: " << format_time(serialize_time) << "\n";
        
        std::string test_str = "Hello, World!";
        double str_serialize_time = measure_perf([&serializer, &test_str]() {
            auto result = serializer.serialize(test_str);
            DoNotOptimize(result);
        }, 50000, 500);
        std::cout << "  Serialize string: " << format_time(str_serialize_time) << "\n";
        
        std::vector<int64_t> test_vec = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        double vec_serialize_time = measure_perf([&serializer, &test_vec]() {
            auto result = serializer.serialize(test_vec);
            DoNotOptimize(result);
        }, 10000, 100);
        std::cout << "  Serialize vector<int64_t>[10]: " << format_time(vec_serialize_time) << "\n";
    }
    
    // CBOR Policy Benchmarks
    std::cout << "\n" << colors::yellow() << "CborPolicy:" << colors::reset() << "\n";
    {
        BinarySerializer<CborPolicy> serializer;
        
        double serialize_time = measure_perf([&serializer, i=0]() mutable {
            auto result = serializer.serialize(static_cast<uint64_t>(i));
            DoNotOptimize(result);
            ++i;
        }, 100000, 1000);
        std::cout << "  Serialize uint64_t: " << format_time(serialize_time) << "\n";
        
        std::string test_str = "Hello, CBOR!";
        double str_serialize_time = measure_perf([&serializer, &test_str]() {
            auto result = serializer.serialize(test_str);
            DoNotOptimize(result);
        }, 50000, 500);
        std::cout << "  Serialize string: " << format_time(str_serialize_time) << "\n";
    }
    
    // Round-trip benchmarks
    std::cout << "\n" << colors::yellow() << "Round-trip Performance:" << colors::reset() << "\n";
    {
        BinarySerializer<CustomBinaryPolicy> serializer;
        uint64_t test_val = 42;
        auto serialized = serializer.serialize(test_val);
        
        double roundtrip_time = measure_perf([&serializer, &serialized]() {
            auto result = serializer.deserialize_uint64(*serialized);
            DoNotOptimize(result);
        }, 100000, 1000);
        std::cout << "  CustomBinary uint64_t: " << format_time(roundtrip_time) << "\n";
    }
    
    {
        BinarySerializer<CborPolicy> serializer;
        uint64_t test_val = 42;
        auto serialized = serializer.serialize(test_val);
        
        double roundtrip_time = measure_perf([&serializer, &serialized]() {
            auto result = serializer.deserialize_uint64(*serialized);
            DoNotOptimize(result);
        }, 100000, 1000);
        std::cout << "  CBOR uint64_t: " << format_time(roundtrip_time) << "\n";
    }
}

bool test_BinarySerializer() {

    PRINT_HEADER(BINARY SERIALIZER)

    TestRunner runner;

    // CustomBinaryPolicy Tests
    RUN_TEST(runner, custom_uint64);
    RUN_TEST(runner, custom_int64);
    RUN_TEST(runner, custom_bool);
    RUN_TEST(runner, custom_double);
    RUN_TEST(runner, custom_string);
    RUN_TEST(runner, custom_empty_string);
    RUN_TEST(runner, custom_vector_int);
    RUN_TEST(runner, custom_vector_string);
    RUN_TEST(runner, custom_empty_vector);

    // CborPolicy Tests
    RUN_TEST(runner, cbor_uint64);
    RUN_TEST(runner, cbor_int64_negative);
    RUN_TEST(runner, cbor_bool);
    RUN_TEST(runner, cbor_double);
    RUN_TEST(runner, cbor_string);
    RUN_TEST(runner, cbor_vector_int);
    RUN_TEST(runner, cbor_compact_encoding);

    benchmark_binaryserializer();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing
