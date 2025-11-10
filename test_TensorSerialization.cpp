#include <iostream>
#include "BinarySerializer.h"
#include "BinarySerializer_Tensor.h"
#include "Tensor.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

// Test Tensor serialization with CustomBinaryPolicy
bool test_tensor_custom_binary() {
    // Create a simple 2x3 tensor
    Tensor<double> original({2, 3});
    original(0, 0) = 1.0;
    original(0, 1) = 2.0;
    original(0, 2) = 3.0;
    original(1, 0) = 4.0;
    original(1, 1) = 5.0;
    original(1, 2) = 6.0;
    
    // Serialize
    auto serialized = serialize_tensor(original, CustomBinaryPolicy{});
    SIMPLE_ASSERT(serialized.has_value(), "Tensor serialization should succeed");
    SIMPLE_ASSERT(!serialized->empty(), "Serialized tensor data should not be empty");
    
    // Deserialize
    auto deserialized = deserialize_tensor<double>(*serialized, CustomBinaryPolicy{});
    SIMPLE_ASSERT(deserialized.has_value(), "Tensor deserialization should succeed");
    
    // Verify shape
    SIMPLE_ASSERT(deserialized->shape() == original.shape(), "Shape should match");
    SIMPLE_ASSERT(deserialized->size() == original.size(), "Size should match");
    
    // Verify data
    SIMPLE_ASSERT((*deserialized)(0, 0) == 1.0, "Element (0,0) should match");
    SIMPLE_ASSERT((*deserialized)(0, 1) == 2.0, "Element (0,1) should match");
    SIMPLE_ASSERT((*deserialized)(0, 2) == 3.0, "Element (0,2) should match");
    SIMPLE_ASSERT((*deserialized)(1, 0) == 4.0, "Element (1,0) should match");
    SIMPLE_ASSERT((*deserialized)(1, 1) == 5.0, "Element (1,1) should match");
    SIMPLE_ASSERT((*deserialized)(1, 2) == 6.0, "Element (1,2) should match");
    
    return true;
}

// Test Tensor serialization with CborPolicy
bool test_tensor_cbor() {
    // Create a 3x2 tensor
    Tensor<float> original({3, 2});
    original(0, 0) = 1.5f;
    original(0, 1) = 2.5f;
    original(1, 0) = 3.5f;
    original(1, 1) = 4.5f;
    original(2, 0) = 5.5f;
    original(2, 1) = 6.5f;
    
    // Serialize
    auto serialized = serialize_tensor(original, CborPolicy{});
    SIMPLE_ASSERT(serialized.has_value(), "CBOR tensor serialization should succeed");
    SIMPLE_ASSERT(!serialized->empty(), "Serialized CBOR tensor data should not be empty");
    
    // Deserialize
    auto deserialized = deserialize_tensor<float>(*serialized, CborPolicy{});
    SIMPLE_ASSERT(deserialized.has_value(), "CBOR tensor deserialization should succeed");
    
    // Verify shape
    SIMPLE_ASSERT(deserialized->shape() == original.shape(), "CBOR shape should match");
    SIMPLE_ASSERT(deserialized->size() == original.size(), "CBOR size should match");
    
    // Verify data
    SIMPLE_ASSERT((*deserialized)(0, 0) == 1.5f, "CBOR Element (0,0) should match");
    SIMPLE_ASSERT((*deserialized)(2, 1) == 6.5f, "CBOR Element (2,1) should match");
    
    return true;
}

// Test 1D tensor
bool test_tensor_1d() {
    Tensor<int64_t> original({5});
    for (size_t i = 0; i < 5; ++i) {
        original(i) = static_cast<int64_t>(i * 10);
    }
    
    auto serialized = serialize_tensor(original, CustomBinaryPolicy{});
    SIMPLE_ASSERT(serialized.has_value(), "1D tensor serialization should succeed");
    
    auto deserialized = deserialize_tensor<int64_t>(*serialized, CustomBinaryPolicy{});
    SIMPLE_ASSERT(deserialized.has_value(), "1D tensor deserialization should succeed");
    SIMPLE_ASSERT(deserialized->ndim() == 1, "Should be 1D");
    SIMPLE_ASSERT((*deserialized)(2) == 20, "Element should match");
    
    return true;
}

// Test 3D tensor
bool test_tensor_3d() {
    Tensor<double> original({2, 2, 2});
    double val = 1.0;
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            for (size_t k = 0; k < 2; ++k) {
                original(i, j, k) = val++;
            }
        }
    }
    
    auto serialized = serialize_tensor(original, CborPolicy{});
    SIMPLE_ASSERT(serialized.has_value(), "3D tensor serialization should succeed");
    
    auto deserialized = deserialize_tensor<double>(*serialized, CborPolicy{});
    SIMPLE_ASSERT(deserialized.has_value(), "3D tensor deserialization should succeed");
    SIMPLE_ASSERT(deserialized->ndim() == 3, "Should be 3D");
    SIMPLE_ASSERT((*deserialized)(1, 1, 1) == 8.0, "Element (1,1,1) should match");
    
    return true;
}

// Test empty tensor
bool test_tensor_empty() {
    Tensor<double> original({0});
    
    auto serialized = serialize_tensor(original, CustomBinaryPolicy{});
    SIMPLE_ASSERT(serialized.has_value(), "Empty tensor serialization should succeed");
    
    auto deserialized = deserialize_tensor<double>(*serialized, CustomBinaryPolicy{});
    SIMPLE_ASSERT(deserialized.has_value(), "Empty tensor deserialization should succeed");
    SIMPLE_ASSERT(deserialized->empty(), "Should be empty");
    
    return true;
}

void benchmark_tensor_serialization() {
    std::cout << "\n" << colors::cyan() << "Tensor Serialization Benchmarks:" << colors::reset() << "\n\n";
    
    // Small tensor
    {
        Tensor<double> small({10, 10});
        for (size_t i = 0; i < 100; ++i) {
            small.data()[i] = static_cast<double>(i);
        }
        
        double time = measure_perf([&small]() {
            auto result = serialize_tensor(small, CustomBinaryPolicy{});
            DoNotOptimize(result);
        }, 10000, 100);
        std::cout << "  CustomBinary 10x10 tensor: " << format_time(time) << "\n";
    }
    
    // Medium tensor
    {
        Tensor<float> medium({100, 100});
        for (size_t i = 0; i < 10000; ++i) {
            medium.data()[i] = static_cast<float>(i);
        }
        
        double time = measure_perf([&medium]() {
            auto result = serialize_tensor(medium, CborPolicy{});
            DoNotOptimize(result);
        }, 1000, 10);
        std::cout << "  CBOR 100x100 tensor: " << format_time(time) << "\n";
    }
}

bool test_TensorSerialization() {
    PRINT_HEADER(BINARY SERIALIZER - TENSOR INTEGRATION)
    
    TestRunner runner;
    
    RUN_TEST(runner, tensor_custom_binary);
    RUN_TEST(runner, tensor_cbor);
    RUN_TEST(runner, tensor_1d);
    RUN_TEST(runner, tensor_3d);
    RUN_TEST(runner, tensor_empty);
    
    benchmark_tensor_serialization();
    
    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing
