/**
 * @file test_TensorSerializer.cpp
 * @brief Comprehensive unit tests for TensorSerializer.h
 */
/*
FATP_META:
  meta_version: 1
  component: TensorSerializer
  file_role: test
  path: tests/test_TensorSerializer.cpp
  namespace: fat_p
  summary: "Unit tests for TensorSerializer."
  related:
    docs_search: "TensorSerializer"
    headers:
      - fat_p/TensorSerializer.h
      - fat_p/Tensor.h
      - fat_p/FatPTest.h
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

#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "FatPTest.h"
#include "Tensor.h"
#include "TensorSerializer.h"

namespace fat_p::testing::tensorserializer
{

// ============================================================================
// Type Roundtrips
// ============================================================================

FATP_TEST_CASE(int8_roundtrip)
{
    Tensor<std::int8_t> t({3, 4});
    std::int8_t val = -128;
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        t.data()[i] = val++;
    }

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<std::int8_t>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    FATP_ASSERT_EQ(deserialized->ndim(), t.ndim(), "ndim mismatch");
    FATP_ASSERT_EQ(deserialized->size(), t.size(), "size mismatch");
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        FATP_ASSERT_EQ(deserialized->data()[i], t.data()[i], "int8 element mismatch");
    }

    return true;
}

FATP_TEST_CASE(uint8_roundtrip)
{
    Tensor<std::uint8_t> t({2, 3, 4});
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        t.data()[i] = static_cast<std::uint8_t>(i % 256);
    }

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<std::uint8_t>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    FATP_ASSERT_EQ(deserialized->size(), t.size(), "size mismatch");
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        FATP_ASSERT_EQ(deserialized->data()[i], t.data()[i], "uint8 element mismatch");
    }

    return true;
}

FATP_TEST_CASE(int16_roundtrip)
{
    Tensor<std::int16_t> t({5, 5});
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        t.data()[i] = static_cast<std::int16_t>(i * 100 - 12500);
    }

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<std::int16_t>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    for (std::size_t i = 0; i < t.size(); ++i)
    {
        FATP_ASSERT_EQ(deserialized->data()[i], t.data()[i], "int16 element mismatch");
    }

    return true;
}

FATP_TEST_CASE(uint16_roundtrip)
{
    Tensor<std::uint16_t> t({10});
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        t.data()[i] = static_cast<std::uint16_t>(i * 6553);
    }

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<std::uint16_t>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    for (std::size_t i = 0; i < t.size(); ++i)
    {
        FATP_ASSERT_EQ(deserialized->data()[i], t.data()[i], "uint16 element mismatch");
    }

    return true;
}

FATP_TEST_CASE(int32_roundtrip)
{
    Tensor<std::int32_t> t({4, 4});
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        t.data()[i] = static_cast<std::int32_t>(i * 100000 - 800000);
    }

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<std::int32_t>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    for (std::size_t i = 0; i < t.size(); ++i)
    {
        FATP_ASSERT_EQ(deserialized->data()[i], t.data()[i], "int32 element mismatch");
    }

    return true;
}

FATP_TEST_CASE(uint32_roundtrip)
{
    Tensor<std::uint32_t> t({3, 3, 3});
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        t.data()[i] = static_cast<std::uint32_t>(i * 159072862UL);
    }

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<std::uint32_t>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    for (std::size_t i = 0; i < t.size(); ++i)
    {
        FATP_ASSERT_EQ(deserialized->data()[i], t.data()[i], "uint32 element mismatch");
    }

    return true;
}

FATP_TEST_CASE(int64_roundtrip)
{
    Tensor<std::int64_t> t({2, 5});
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        t.data()[i] = static_cast<std::int64_t>(i) * 1234567890123LL - 5000000000000LL;
    }

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<std::int64_t>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    for (std::size_t i = 0; i < t.size(); ++i)
    {
        FATP_ASSERT_EQ(deserialized->data()[i], t.data()[i], "int64 element mismatch");
    }

    return true;
}

FATP_TEST_CASE(uint64_roundtrip)
{
    Tensor<std::uint64_t> t({6});
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        t.data()[i] = static_cast<std::uint64_t>(i) * 3074457345618258602ULL;
    }

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<std::uint64_t>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    for (std::size_t i = 0; i < t.size(); ++i)
    {
        FATP_ASSERT_EQ(deserialized->data()[i], t.data()[i], "uint64 element mismatch");
    }

    return true;
}

FATP_TEST_CASE(float_roundtrip)
{
    Tensor<float> t({4, 4});
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        t.data()[i] = static_cast<float>(i) * 0.125f - 1.0f;
    }

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<float>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    for (std::size_t i = 0; i < t.size(); ++i)
    {
        FATP_ASSERT_EQ(deserialized->data()[i], t.data()[i], "float element mismatch");
    }

    return true;
}

FATP_TEST_CASE(double_roundtrip)
{
    Tensor<double> t({3, 3});
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        t.data()[i] = static_cast<double>(i) * 3.14159265358979 - 10.0;
    }

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<double>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    for (std::size_t i = 0; i < t.size(); ++i)
    {
        FATP_ASSERT_EQ(deserialized->data()[i], t.data()[i], "double element mismatch");
    }

    return true;
}

// ============================================================================
// Shape Tests
// ============================================================================

FATP_TEST_CASE(shape_1d)
{
    Tensor<int> t({100});
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        t.data()[i] = static_cast<int>(i);
    }

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<int>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    FATP_ASSERT_EQ(deserialized->ndim(), 1U, "should be 1D");
    FATP_ASSERT_EQ(deserialized->shape()[0], 100U, "dim 0 should be 100");

    return true;
}

FATP_TEST_CASE(shape_2d)
{
    Tensor<int> t({10, 20});
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        t.data()[i] = static_cast<int>(i);
    }

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<int>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    FATP_ASSERT_EQ(deserialized->ndim(), 2U, "should be 2D");
    FATP_ASSERT_EQ(deserialized->shape()[0], 10U, "dim 0");
    FATP_ASSERT_EQ(deserialized->shape()[1], 20U, "dim 1");

    return true;
}

FATP_TEST_CASE(shape_3d)
{
    Tensor<float> t({2, 3, 4});
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        t.data()[i] = static_cast<float>(i);
    }

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<float>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    FATP_ASSERT_EQ(deserialized->ndim(), 3U, "should be 3D");
    FATP_ASSERT_EQ(deserialized->shape()[0], 2U, "dim 0");
    FATP_ASSERT_EQ(deserialized->shape()[1], 3U, "dim 1");
    FATP_ASSERT_EQ(deserialized->shape()[2], 4U, "dim 2");

    return true;
}

FATP_TEST_CASE(shape_4d)
{
    Tensor<double> t({2, 2, 2, 2});
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        t.data()[i] = static_cast<double>(i);
    }

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<double>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    FATP_ASSERT_EQ(deserialized->ndim(), 4U, "should be 4D");
    FATP_ASSERT_EQ(deserialized->size(), 16U, "total size should be 16");

    return true;
}

FATP_TEST_CASE(single_element)
{
    Tensor<double> t({1});
    t.data()[0] = 42.5;

    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), serialized.error().message.c_str());

    auto deserialized = deserialize_tensor<double>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), deserialized.error().message.c_str());

    FATP_ASSERT_EQ(deserialized->size(), 1U, "single element");
    FATP_ASSERT_EQ(deserialized->data()[0], 42.5, "value preserved");

    return true;
}

// ============================================================================
// Malformed Input Tests
// ============================================================================

FATP_TEST_CASE(truncated_header)
{
    std::vector<std::uint8_t> truncated = {0x54, 0x4E, 0x53};

    auto result = deserialize_tensor<int>(truncated);
    FATP_ASSERT_TRUE(!result.has_value(), "Truncated header should fail");

    return true;
}

FATP_TEST_CASE(wrong_magic)
{
    Tensor<int> t({2, 2});
    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), "Serialize should succeed");

    (*serialized)[0] = 0x00;

    auto result = deserialize_tensor<int>(*serialized);
    FATP_ASSERT_TRUE(!result.has_value(), "Wrong magic should fail");

    return true;
}

FATP_TEST_CASE(wrong_version)
{
    Tensor<int> t({2, 2});
    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), "Serialize should succeed");

    (*serialized)[4] = 99;

    auto result = deserialize_tensor<int>(*serialized);
    FATP_ASSERT_TRUE(!result.has_value(), "Wrong version should fail");

    return true;
}

FATP_TEST_CASE(type_mismatch)
{
    Tensor<float> t({3, 3});
    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), "Serialize should succeed");

    auto result = deserialize_tensor<double>(*serialized);
    FATP_ASSERT_TRUE(!result.has_value(), "Type mismatch should fail");

    return true;
}

FATP_TEST_CASE(invalid_ndim_zero)
{
    Tensor<int> t({2, 2});
    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), "Serialize should succeed");

    (*serialized)[6] = 0;
    (*serialized)[7] = 0;

    auto result = deserialize_tensor<int>(*serialized);
    FATP_ASSERT_TRUE(!result.has_value(), "Zero ndim should fail");

    return true;
}

FATP_TEST_CASE(invalid_ndim_too_large)
{
    Tensor<int> t({2, 2});
    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), "Serialize should succeed");

    (*serialized)[6] = 0;
    (*serialized)[7] = 100;

    auto result = deserialize_tensor<int>(*serialized);
    FATP_ASSERT_TRUE(!result.has_value(), "ndim > 32 should fail");

    return true;
}

FATP_TEST_CASE(truncated_data)
{
    Tensor<double> t({10, 10});
    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), "Serialize should succeed");

    serialized->resize(serialized->size() / 2);

    auto result = deserialize_tensor<double>(*serialized);
    FATP_ASSERT_TRUE(!result.has_value(), "Truncated data should fail");

    return true;
}

FATP_TEST_CASE(empty_buffer)
{
    std::vector<std::uint8_t> empty;

    auto result = deserialize_tensor<int>(empty);
    FATP_ASSERT_TRUE(!result.has_value(), "Empty buffer should fail");

    return true;
}

// ============================================================================
// Fuzz Tests
// ============================================================================

FATP_TEST_CASE(fuzz_float_2d)
{
    std::mt19937_64 rng(0x7E50A1B2C3D4E5F6ULL);
    std::uniform_real_distribution<float> dist(-1e6f, 1e6f);

    for (int iter = 0; iter < 100; ++iter)
    {
        std::uniform_int_distribution<std::size_t> dim_dist(1, 20);
        const std::size_t rows = dim_dist(rng);
        const std::size_t cols = dim_dist(rng);

        Tensor<float> t({rows, cols});
        for (std::size_t i = 0; i < t.size(); ++i)
        {
            t.data()[i] = dist(rng);
        }

        auto serialized = serialize_tensor(t);
        FATP_ASSERT_TRUE(serialized.has_value(), "Fuzz serialize failed");

        auto deserialized = deserialize_tensor<float>(*serialized);
        FATP_ASSERT_TRUE(deserialized.has_value(), "Fuzz deserialize failed");

        FATP_ASSERT_EQ(deserialized->size(), t.size(), "Fuzz size mismatch");
        for (std::size_t i = 0; i < t.size(); ++i)
        {
            FATP_ASSERT_EQ(deserialized->data()[i], t.data()[i], "Fuzz element mismatch");
        }
    }

    return true;
}

FATP_TEST_CASE(fuzz_double_3d)
{
    std::mt19937_64 rng(0x7E50A1B2C3D4E5F7ULL);
    std::uniform_real_distribution<double> dist(-1e10, 1e10);

    for (int iter = 0; iter < 50; ++iter)
    {
        std::uniform_int_distribution<std::size_t> dim_dist(1, 10);
        const std::size_t d0 = dim_dist(rng);
        const std::size_t d1 = dim_dist(rng);
        const std::size_t d2 = dim_dist(rng);

        Tensor<double> t({d0, d1, d2});
        for (std::size_t i = 0; i < t.size(); ++i)
        {
            t.data()[i] = dist(rng);
        }

        auto serialized = serialize_tensor(t);
        FATP_ASSERT_TRUE(serialized.has_value(), "Fuzz serialize failed");

        auto deserialized = deserialize_tensor<double>(*serialized);
        FATP_ASSERT_TRUE(deserialized.has_value(), "Fuzz deserialize failed");

        for (std::size_t i = 0; i < t.size(); ++i)
        {
            FATP_ASSERT_EQ(deserialized->data()[i], t.data()[i], "Fuzz element mismatch");
        }
    }

    return true;
}

FATP_TEST_CASE(fuzz_int32_various_shapes)
{
    std::mt19937_64 rng(0x7E50A1B2C3D4E5F8ULL);
    std::uniform_int_distribution<std::int32_t> val_dist(std::numeric_limits<std::int32_t>::min(),
                                                         std::numeric_limits<std::int32_t>::max());

    for (int iter = 0; iter < 100; ++iter)
    {
        std::uniform_int_distribution<std::size_t> ndim_dist(1, 5);
        const std::size_t ndim = ndim_dist(rng);

        std::vector<std::size_t> shape(ndim);
        std::uniform_int_distribution<std::size_t> dim_dist(1, 8);
        for (std::size_t i = 0; i < ndim; ++i)
        {
            shape[i] = dim_dist(rng);
        }

        Tensor<std::int32_t> t(shape);
        for (std::size_t i = 0; i < t.size(); ++i)
        {
            t.data()[i] = val_dist(rng);
        }

        auto serialized = serialize_tensor(t);
        FATP_ASSERT_TRUE(serialized.has_value(), "Fuzz serialize failed");

        auto deserialized = deserialize_tensor<std::int32_t>(*serialized);
        FATP_ASSERT_TRUE(deserialized.has_value(), "Fuzz deserialize failed");

        FATP_ASSERT_EQ(deserialized->ndim(), t.ndim(), "Fuzz ndim mismatch");
        for (std::size_t i = 0; i < t.size(); ++i)
        {
            FATP_ASSERT_EQ(deserialized->data()[i], t.data()[i], "Fuzz element mismatch");
        }
    }

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_tensorserializer()
{
    std::cout << "\n" << colors::cyan() << "TensorSerializer Benchmarks:" << colors::reset() << "\n\n";

    Tensor<float> small_f({64, 64});
    for (std::size_t i = 0; i < small_f.size(); ++i)
    {
        small_f.data()[i] = static_cast<float>(i) * 0.001f;
    }

    double small_enc_time = measure_perf(
        [&small_f]() {
            auto r = serialize_tensor(small_f);
            DoNotOptimize(r);
        },
        1000,
        100);
    std::cout << "Serialize float[64x64]: " << format_time(small_enc_time) << "\n";

    auto small_buf = serialize_tensor(small_f);

    double small_dec_time = measure_perf(
        [&small_buf]() {
            auto r = deserialize_tensor<float>(*small_buf);
            DoNotOptimize(r);
        },
        1000,
        100);
    std::cout << "Deserialize float[64x64]: " << format_time(small_dec_time) << "\n";
    std::cout << "Serialized size: " << small_buf->size() << " bytes\n\n";

    Tensor<double> large_d({256, 256});
    for (std::size_t i = 0; i < large_d.size(); ++i)
    {
        large_d.data()[i] = static_cast<double>(i) * 0.001;
    }

    double large_enc_time = measure_perf(
        [&large_d]() {
            auto r = serialize_tensor(large_d);
            DoNotOptimize(r);
        },
        100,
        10);
    std::cout << "Serialize double[256x256]: " << format_time(large_enc_time) << "\n";

    auto large_buf = serialize_tensor(large_d);

    double large_dec_time = measure_perf(
        [&large_buf]() {
            auto r = deserialize_tensor<double>(*large_buf);
            DoNotOptimize(r);
        },
        100,
        10);
    std::cout << "Deserialize double[256x256]: " << format_time(large_dec_time) << "\n";
    std::cout << "Serialized size: " << large_buf->size() << " bytes\n\n";

    Tensor<std::int32_t> int_t({100, 100, 10});
    for (std::size_t i = 0; i < int_t.size(); ++i)
    {
        int_t.data()[i] = static_cast<std::int32_t>(i);
    }

    double int_enc_time = measure_perf(
        [&int_t]() {
            auto r = serialize_tensor(int_t);
            DoNotOptimize(r);
        },
        100,
        10);
    std::cout << "Serialize int32[100x100x10]: " << format_time(int_enc_time) << "\n";

    auto int_buf = serialize_tensor(int_t);

    double int_dec_time = measure_perf(
        [&int_buf]() {
            auto r = deserialize_tensor<std::int32_t>(*int_buf);
            DoNotOptimize(r);
        },
        100,
        10);
    std::cout << "Deserialize int32[100x100x10]: " << format_time(int_dec_time) << "\n";
    std::cout << "Serialized size: " << int_buf->size() << " bytes\n";
}

} // namespace fat_p::testing::tensorserializer

namespace fat_p::testing
{

bool test_TensorSerializer()
{
    FATP_PRINT_HEADER(TENSOR SERIALIZER)

    TestRunner runner;

    // Type roundtrips
    FATP_RUN_TEST_NS(runner, tensorserializer, int8_roundtrip);
    FATP_RUN_TEST_NS(runner, tensorserializer, uint8_roundtrip);
    FATP_RUN_TEST_NS(runner, tensorserializer, int16_roundtrip);
    FATP_RUN_TEST_NS(runner, tensorserializer, uint16_roundtrip);
    FATP_RUN_TEST_NS(runner, tensorserializer, int32_roundtrip);
    FATP_RUN_TEST_NS(runner, tensorserializer, uint32_roundtrip);
    FATP_RUN_TEST_NS(runner, tensorserializer, int64_roundtrip);
    FATP_RUN_TEST_NS(runner, tensorserializer, uint64_roundtrip);
    FATP_RUN_TEST_NS(runner, tensorserializer, float_roundtrip);
    FATP_RUN_TEST_NS(runner, tensorserializer, double_roundtrip);

    // Shape tests
    FATP_RUN_TEST_NS(runner, tensorserializer, shape_1d);
    FATP_RUN_TEST_NS(runner, tensorserializer, shape_2d);
    FATP_RUN_TEST_NS(runner, tensorserializer, shape_3d);
    FATP_RUN_TEST_NS(runner, tensorserializer, shape_4d);
    FATP_RUN_TEST_NS(runner, tensorserializer, single_element);

    // Malformed input
    FATP_RUN_TEST_NS(runner, tensorserializer, truncated_header);
    FATP_RUN_TEST_NS(runner, tensorserializer, wrong_magic);
    FATP_RUN_TEST_NS(runner, tensorserializer, wrong_version);
    FATP_RUN_TEST_NS(runner, tensorserializer, type_mismatch);
    FATP_RUN_TEST_NS(runner, tensorserializer, invalid_ndim_zero);
    FATP_RUN_TEST_NS(runner, tensorserializer, invalid_ndim_too_large);
    FATP_RUN_TEST_NS(runner, tensorserializer, truncated_data);
    FATP_RUN_TEST_NS(runner, tensorserializer, empty_buffer);

    // Fuzz tests
    FATP_RUN_TEST_NS(runner, tensorserializer, fuzz_float_2d);
    FATP_RUN_TEST_NS(runner, tensorserializer, fuzz_double_3d);
    FATP_RUN_TEST_NS(runner, tensorserializer, fuzz_int32_various_shapes);

    tensorserializer::benchmark_tensorserializer();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorSerializer() ? 0 : 1;
}
#endif
