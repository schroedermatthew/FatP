/**
 * @file test_TensorSerializer.cpp
 * @brief Comprehensive unit tests for TensorSerializer.h
 */
/*
FATP_META:
  meta_version: 1
  component: TensorSerializer
  file_role: test
  path: components/Tensor/tests/test_TensorSerializer.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for TensorSerializer."
  api_stability: in_work
  related:
    docs_search: "TensorSerializer"
    headers:
      - include/fat_p/TensorSerializer.h
      - include/fat_p/Tensor.h
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

#include <cstdint>
#include <limits>
#include <new>
#include <random>
#include <type_traits>
#include <vector>

#include "FatPTest.h"
#include "Tensor.h"
#include "TensorSerializer.h"

namespace fat_p::testing::tensorserializer
{

template <typename T>
class CountingSerializerAllocator
{
public:
    using value_type = T;

    CountingSerializerAllocator() = delete;

    explicit CountingSerializerAllocator(int& allocations)
        : mAllocations(&allocations)
    {
    }

    template <typename U>
    CountingSerializerAllocator(const CountingSerializerAllocator<U>& other) noexcept
        : mAllocations(other.counter())
    {
    }

    T* allocate(size_t count)
    {
        ++*mAllocations;
        return std::allocator<T>{}.allocate(count);
    }

    void deallocate(T* data, size_t count) noexcept
    {
        std::allocator<T>{}.deallocate(data, count);
    }

    int* counter() const noexcept
    {
        return mAllocations;
    }

    template <typename U>
    struct rebind
    {
        using other = CountingSerializerAllocator<U>;
    };

    template <typename U>
    bool operator==(const CountingSerializerAllocator<U>& other) const noexcept
    {
        return mAllocations == other.counter();
    }

private:
    template <typename>
    friend class CountingSerializerAllocator;

    int* mAllocations;
};

template <typename T>
class ThrowingDefaultSerializerAllocator
{
public:
    using value_type = T;
    using is_always_equal = std::true_type;

    ThrowingDefaultSerializerAllocator()
    {
        throw std::bad_alloc();
    }

    template <typename U>
    ThrowingDefaultSerializerAllocator(const ThrowingDefaultSerializerAllocator<U>&) noexcept
    {
    }

    T* allocate(size_t count)
    {
        return std::allocator<T>{}.allocate(count);
    }

    void deallocate(T* data, size_t count) noexcept
    {
        std::allocator<T>{}.deallocate(data, count);
    }

    template <typename U>
    struct rebind
    {
        using other = ThrowingDefaultSerializerAllocator<U>;
    };

    template <typename U>
    bool operator==(const ThrowingDefaultSerializerAllocator<U>&) const noexcept
    {
        return true;
    }
};

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

    FATP_ASSERT_EQ(deserialized->rank(), t.rank(), "rank mismatch");
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

    FATP_ASSERT_EQ(deserialized->rank(), 1U, "should be 1D");
    FATP_ASSERT_EQ(deserialized->extent(0), 100U, "dim 0 should be 100");

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

    FATP_ASSERT_EQ(deserialized->rank(), 2U, "should be 2D");
    FATP_ASSERT_EQ(deserialized->extent(0), 10U, "dim 0");
    FATP_ASSERT_EQ(deserialized->extent(1), 20U, "dim 1");

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

    FATP_ASSERT_EQ(deserialized->rank(), 3U, "should be 3D");
    FATP_ASSERT_EQ(deserialized->extent(0), 2U, "dim 0");
    FATP_ASSERT_EQ(deserialized->extent(1), 3U, "dim 1");
    FATP_ASSERT_EQ(deserialized->extent(2), 4U, "dim 2");

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

    FATP_ASSERT_EQ(deserialized->rank(), 4U, "should be 4D");
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

FATP_TEST_CASE(golden_big_endian_bytes)
{
    Tensor<std::int32_t> integer({1});
    integer[0] = 0x01020304;
    auto integer_bytes = serialize_tensor(integer);
    FATP_ASSERT_TRUE(integer_bytes.has_value(), "int32 golden serialization should succeed");

    const std::vector<std::uint8_t> expected_integer = {0x54, 0x4E, 0x53, 0x52, 0x02, 0x05, 0x00, 0x01, 0x00, 0x00,
                                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
                                                        0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x03, 0x04};
    FATP_ASSERT_TRUE(*integer_bytes == expected_integer, "int32 bytes must use canonical big-endian encoding");

    Tensor<float> floating({1});
    floating[0] = 1.0f;
    auto float_bytes = serialize_tensor(floating);
    FATP_ASSERT_TRUE(float_bytes.has_value(), "float golden serialization should succeed");

    const std::vector<std::uint8_t> expected_float = {0x54, 0x4E, 0x53, 0x52, 0x02, 0x09, 0x00, 0x01, 0x00, 0x00,
                                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
                                                      0x00, 0x00, 0x00, 0x01, 0x3F, 0x80, 0x00, 0x00};
    FATP_ASSERT_TRUE(*float_bytes == expected_float, "float bytes must use canonical big-endian encoding");

    return true;
}

FATP_TEST_CASE(non_contiguous_view_roundtrip)
{
    Tensor<std::int32_t> matrix({3, 3});
    for (std::size_t i = 0; i < matrix.size(); ++i)
    {
        matrix[i] = static_cast<std::int32_t>(i);
    }

    auto column = matrix.columnView(1);
    auto serialized = serialize_tensor(column);
    FATP_ASSERT_TRUE(serialized.has_value(), "Column view serialization failed");

    auto deserialized = deserialize_tensor<std::int32_t>(*serialized);
    FATP_ASSERT_TRUE(deserialized.has_value(), "Column view deserialization failed");
    FATP_ASSERT_EQ(deserialized->extent(0), size_t(3), "Column view row count");
    FATP_ASSERT_EQ(deserialized->extent(1), size_t(1), "Column view column count");
    FATP_ASSERT_EQ((*deserialized)[0], std::int32_t(1), "Column view element 0");
    FATP_ASSERT_EQ((*deserialized)[1], std::int32_t(4), "Column view element 1");
    FATP_ASSERT_EQ((*deserialized)[2], std::int32_t(7), "Column view element 2");

    return true;
}

FATP_TEST_CASE(truncated_dimension_metadata)
{
    Tensor<std::int32_t> tensor({2, 2}, 1);
    auto serialized = serialize_tensor(tensor);
    FATP_ASSERT_TRUE(serialized.has_value(), "Setup serialization failed");

    auto truncated = *serialized;
    truncated.resize(8); // Complete fixed header, but no dimensions or strides.
    auto result = deserialize_tensor<std::int32_t>(truncated);
    FATP_ASSERT_FALSE(result.has_value(), "Truncated dimension metadata must return an error");

    for (const size_t cutoff : {size_t(9), size_t(17), size_t(25), size_t(33)})
    {
        auto partial_field = *serialized;
        partial_field.resize(cutoff);
        auto partial_result = deserialize_tensor<std::int32_t>(partial_field);
        FATP_ASSERT_FALSE(partial_result.has_value(), "A partially encoded dimension or stride must return an error");
    }

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

FATP_TEST_CASE(zero_extent_empty_roundtrip)
{
    Tensor<int> t;
    auto serialized = serialize_tensor(t);
    FATP_ASSERT_TRUE(serialized.has_value(), "Empty tensor serialization should succeed");

    const std::vector<std::uint8_t> expected = {0x54, 0x4E, 0x53, 0x52, 0x02, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
                                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    FATP_ASSERT_TRUE(*serialized == expected, "Canonical empty tensor should have exact version-2 bytes");

    auto result = deserialize_tensor<int>(*serialized);
    FATP_ASSERT_TRUE(result.has_value(), "Empty tensor deserialization should succeed");
    FATP_ASSERT_EQ(result->rank(), size_t(1), "Canonical empty tensor rank should round-trip");
    FATP_ASSERT_TRUE(result->extents() == DynamicExtents{0}, "Canonical empty shape should round-trip");
    FATP_ASSERT_EQ(result->size(), size_t(0), "Empty tensor size should round-trip");
    FATP_ASSERT_EQ(result->data(), nullptr, "Empty tensor data should remain null");

    return true;
}

FATP_TEST_CASE(rank_zero_scalar_roundtrip)
{
    Tensor<std::int32_t> scalar(std::vector<size_t>{}, 42);
    auto serialized = serialize_tensor(scalar);
    FATP_ASSERT_TRUE(serialized.has_value(), "Rank-zero scalar serialization should succeed");

    const std::vector<std::uint8_t> expected = {0x54, 0x4E, 0x53, 0x52, 0x02, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A};
    FATP_ASSERT_TRUE(*serialized == expected, "Rank-zero scalar should have canonical version-2 bytes");

    auto result = deserialize_tensor<std::int32_t>(*serialized);
    FATP_ASSERT_TRUE(result.has_value(), "Rank-zero scalar deserialization should succeed");
    FATP_ASSERT_EQ(result->rank(), size_t(0), "Rank-zero scalar rank should round-trip");
    FATP_ASSERT_EQ(result->size(), size_t(1), "Rank-zero scalar should retain its single element");
    FATP_ASSERT_FALSE(result->empty(), "Rank-zero scalar should remain non-empty");
    FATP_ASSERT_EQ((*result)(), 42, "Rank-zero scalar value should round-trip");

    return true;
}

FATP_TEST_CASE(rank_zero_scalar_payload_validation)
{
    Tensor<std::int32_t> scalar(std::vector<size_t>{}, 42);
    auto serialized = serialize_tensor(scalar);
    FATP_ASSERT_TRUE(serialized.has_value(), "Scalar setup serialization should succeed");

    auto missing_payload = *serialized;
    missing_payload.pop_back();
    FATP_ASSERT_FALSE(deserialize_tensor<std::int32_t>(missing_payload).has_value(),
                      "A rank-zero scalar with a truncated payload must be rejected");

    auto extra_payload = *serialized;
    extra_payload.push_back(0x00);
    FATP_ASSERT_FALSE(deserialize_tensor<std::int32_t>(extra_payload).has_value(),
                      "A rank-zero scalar with trailing payload bytes must be rejected");

    return true;
}

FATP_TEST_CASE(legacy_v1_rank_zero_empty_rejected)
{
    const std::vector<std::uint8_t> legacy_v1_empty = {0x54, 0x4E, 0x53, 0x52, 0x01, 0x05, 0x00, 0x00};
    auto result = deserialize_tensor<std::int32_t>(legacy_v1_empty);
    FATP_ASSERT_FALSE(result.has_value(), "Version-1 rank-zero empty bytes must not be reinterpreted as a scalar");

    return true;
}

FATP_TEST_CASE(large_extent_before_zero_roundtrip)
{
    const std::vector<size_t> shape = {std::numeric_limits<size_t>::max(), size_t(2), size_t(0)};
    Tensor<std::int32_t> tensor(shape);
    FATP_ASSERT_TRUE(tensor.empty(), "A trailing zero extent should make an adversarial shape empty");

    auto serialized = serialize_tensor(tensor);
    FATP_ASSERT_TRUE(serialized.has_value(), "Large extents before a zero should serialize without a payload");

    auto result = deserialize_tensor<std::int32_t>(*serialized);
    FATP_ASSERT_TRUE(result.has_value(), "Deserializer should detect zero extents before multiplying dimensions");
    FATP_ASSERT_TRUE(result->extents().values() == shape, "Large zero-extent shape should round-trip exactly");
    FATP_ASSERT_TRUE(result->empty(), "Large zero-extent round-trip should remain empty");

    return true;
}

FATP_TEST_CASE(rank_limit_serialization)
{
    Tensor<std::int32_t> rank_32(std::vector<size_t>(TENSOR_MAX_DIMENSIONS, size_t(1)), 7);
    auto accepted = serialize_tensor(rank_32);
    FATP_ASSERT_TRUE(accepted.has_value(), "The maximum supported rank should serialize");
    auto roundtrip = deserialize_tensor<std::int32_t>(*accepted);
    FATP_ASSERT_TRUE(roundtrip.has_value(), "The maximum supported rank should deserialize");
    FATP_ASSERT_EQ(roundtrip->rank(), TENSOR_MAX_DIMENSIONS, "Rank 32 should round-trip exactly");

    Tensor<std::int32_t> rank_33(std::vector<size_t>(TENSOR_MAX_DIMENSIONS + 1, size_t(1)), 7);
    FATP_ASSERT_FALSE(serialize_tensor(rank_33).has_value(), "Ranks above the wire-format limit must be rejected");

    return true;
}

FATP_TEST_CASE(deserialization_resource_limits)
{
    Tensor<std::int32_t> tensor({2, 2}, 7);
    auto serialized = serialize_tensor(tensor);
    FATP_ASSERT_TRUE(serialized.has_value(), "Resource-limit setup serialization should succeed");

    TensorDeserializationLimits element_limit;
    element_limit.max_elements = 3;
    auto element_rejected = deserialize_tensor<std::int32_t>(*serialized, element_limit);
    FATP_ASSERT_FALSE(element_rejected.has_value(),
                      "Deserializer should reject an element count above the caller budget");
    FATP_ASSERT_TRUE(element_rejected.error().code == TensorSerializationErrorCode::ResourceLimit,
                     "Element budget rejection should have a structured resource-limit code");

    TensorDeserializationLimits byte_limit;
    byte_limit.max_payload_bytes = 3 * sizeof(std::int32_t);
    FATP_ASSERT_FALSE(deserialize_tensor<std::int32_t>(*serialized, byte_limit).has_value(),
                      "Deserializer should reject a payload above the caller byte budget");

    TensorDeserializationLimits rank_limit;
    rank_limit.max_dimensions = 1;
    auto rank_rejected = deserialize_tensor<std::int32_t>(*serialized, rank_limit);
    FATP_ASSERT_FALSE(rank_rejected.has_value(), "Deserializer should reject a rank above the caller budget");
    FATP_ASSERT_TRUE(rank_rejected.error().code == TensorSerializationErrorCode::ResourceLimit,
                     "Rank budget rejection should be distinguishable from malformed rank metadata");

    TensorDeserializationLimits extent_limit;
    extent_limit.max_extent = 1;
    auto extent_rejected = deserialize_tensor<std::int32_t>(*serialized, extent_limit);
    FATP_ASSERT_FALSE(extent_rejected.has_value(), "Deserializer should reject an extent above the caller budget");
    FATP_ASSERT_TRUE(extent_rejected.error().code == TensorSerializationErrorCode::ResourceLimit,
                     "Extent budget rejection should have a structured resource-limit code");

    TensorDeserializationLimits exact_limit;
    exact_limit.max_dimensions = 2;
    exact_limit.max_extent = 2;
    exact_limit.max_elements = 4;
    exact_limit.max_payload_bytes = 4 * sizeof(std::int32_t);
    FATP_ASSERT_TRUE(deserialize_tensor<std::int32_t>(*serialized, exact_limit).has_value(),
                     "Values exactly on every deserialization boundary should be accepted");

    int allocation_count = 0;
    CountingSerializerAllocator<std::int32_t> counting_allocator(allocation_count);
    auto rejected_without_allocation =
        deserialize_tensor<std::int32_t, CountingSerializerAllocator<std::int32_t>>(*serialized,
                                                                                    counting_allocator,
                                                                                    element_limit);
    FATP_ASSERT_FALSE(rejected_without_allocation.has_value(), "Counting allocator setup should reject the payload");
    FATP_ASSERT_EQ(allocation_count, 0, "Resource rejection must occur before Tensor element-storage allocation");

    auto accepted_with_allocator =
        deserialize_tensor<std::int32_t, CountingSerializerAllocator<std::int32_t>>(*serialized,
                                                                                    counting_allocator,
                                                                                    exact_limit);
    FATP_ASSERT_TRUE(accepted_with_allocator.has_value(), "Allocator-instance overload should accept valid input");
    FATP_ASSERT_EQ(allocation_count, 1, "Accepted deserialization should allocate through the supplied allocator");

    Tensor<std::int32_t> empty;
    auto empty_bytes = serialize_tensor(empty);
    FATP_ASSERT_TRUE(empty_bytes.has_value(), "Zero-budget empty setup should serialize");
    TensorDeserializationLimits zero_payload_budget;
    zero_payload_budget.max_dimensions = 1;
    zero_payload_budget.max_elements = 0;
    zero_payload_budget.max_payload_bytes = 0;
    FATP_ASSERT_TRUE(deserialize_tensor<std::int32_t>(*empty_bytes, zero_payload_budget).has_value(),
                     "An empty tensor should be admitted by a zero element/payload budget");

    Tensor<std::int32_t> scalar(std::vector<size_t>{}, 1);
    auto scalar_bytes = serialize_tensor(scalar);
    FATP_ASSERT_TRUE(scalar_bytes.has_value(), "Zero-budget scalar setup should serialize");
    FATP_ASSERT_FALSE(deserialize_tensor<std::int32_t>(*scalar_bytes, zero_payload_budget).has_value(),
                      "A rank-zero scalar should be rejected by a zero element/payload budget");

    FATP_ASSERT_TRUE(deserialize_tensor<std::int32_t>(*serialized).has_value(),
                     "Default resource limits should admit ordinary tensors");

    auto allocator_rejected =
        deserialize_tensor<std::int32_t, ThrowingDefaultSerializerAllocator<std::int32_t>>(*serialized);
    FATP_ASSERT_FALSE(allocator_rejected.has_value(), "A throwing default allocator should be reported as an error");
    FATP_ASSERT_TRUE(allocator_rejected.error().code == TensorSerializationErrorCode::AllocationFailure,
                     "Default allocator construction failures should have a structured allocation-failure code");

    return true;
}

FATP_TEST_CASE(noncanonical_strides)
{
    Tensor<std::int32_t> tensor({2, 2}, 1);
    auto serialized = serialize_tensor(tensor);
    FATP_ASSERT_TRUE(serialized.has_value(), "Setup serialization should succeed");

    constexpr size_t first_stride_last_byte = 8 + 2 * 8 + 7;
    (*serialized)[first_stride_last_byte] = 3;
    auto result = deserialize_tensor<std::int32_t>(*serialized);
    FATP_ASSERT_FALSE(result.has_value(), "Noncanonical serialized strides must be rejected");

    return true;
}

FATP_TEST_CASE(trailing_data)
{
    Tensor<std::int32_t> tensor({1}, 7);
    auto serialized = serialize_tensor(tensor);
    FATP_ASSERT_TRUE(serialized.has_value(), "Setup serialization should succeed");

    serialized->push_back(0xFF);
    auto result = deserialize_tensor<std::int32_t>(*serialized);
    FATP_ASSERT_FALSE(result.has_value(), "Trailing tensor bytes must be rejected");

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

        FATP_ASSERT_EQ(deserialized->rank(), t.rank(), "Fuzz rank mismatch");
        for (std::size_t i = 0; i < t.size(); ++i)
        {
            FATP_ASSERT_EQ(deserialized->data()[i], t.data()[i], "Fuzz element mismatch");
        }
    }

    return true;
}

} // namespace fat_p::testing::tensorserializer

// ============================================================================

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
    FATP_RUN_TEST_NS(runner, tensorserializer, golden_big_endian_bytes);
    FATP_RUN_TEST_NS(runner, tensorserializer, non_contiguous_view_roundtrip);

    // Malformed input
    FATP_RUN_TEST_NS(runner, tensorserializer, truncated_header);
    FATP_RUN_TEST_NS(runner, tensorserializer, truncated_dimension_metadata);
    FATP_RUN_TEST_NS(runner, tensorserializer, wrong_magic);
    FATP_RUN_TEST_NS(runner, tensorserializer, wrong_version);
    FATP_RUN_TEST_NS(runner, tensorserializer, type_mismatch);
    FATP_RUN_TEST_NS(runner, tensorserializer, zero_extent_empty_roundtrip);
    FATP_RUN_TEST_NS(runner, tensorserializer, rank_zero_scalar_roundtrip);
    FATP_RUN_TEST_NS(runner, tensorserializer, rank_zero_scalar_payload_validation);
    FATP_RUN_TEST_NS(runner, tensorserializer, legacy_v1_rank_zero_empty_rejected);
    FATP_RUN_TEST_NS(runner, tensorserializer, large_extent_before_zero_roundtrip);
    FATP_RUN_TEST_NS(runner, tensorserializer, rank_limit_serialization);
    FATP_RUN_TEST_NS(runner, tensorserializer, deserialization_resource_limits);
    FATP_RUN_TEST_NS(runner, tensorserializer, invalid_ndim_too_large);
    FATP_RUN_TEST_NS(runner, tensorserializer, noncanonical_strides);
    FATP_RUN_TEST_NS(runner, tensorserializer, trailing_data);
    FATP_RUN_TEST_NS(runner, tensorserializer, truncated_data);
    FATP_RUN_TEST_NS(runner, tensorserializer, empty_buffer);

    // Fuzz tests
    FATP_RUN_TEST_NS(runner, tensorserializer, fuzz_float_2d);
    FATP_RUN_TEST_NS(runner, tensorserializer, fuzz_double_3d);
    FATP_RUN_TEST_NS(runner, tensorserializer, fuzz_int32_various_shapes);


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
