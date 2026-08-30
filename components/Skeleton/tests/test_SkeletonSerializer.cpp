/**
 * @file test_SkeletonSerializer.cpp
 * @brief Unit tests for SkeletonSerializer.h.
 */

/*
FATP_META:
  meta_version: 1
  component: Skeleton
  file_role: test
  path: components/Skeleton/tests/test_SkeletonSerializer.cpp
  namespace: fat_p::testing::skeletonserializer
  layer: Testing
  summary: Unit tests for skeleton binary stream serialization.
  api_stability: in_work
  related:
    headers:
      - include/fat_p/SkeletonSerializer.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: codex
    mode: manual
*/

#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "SkeletonSerializer.h"

#include "FatPTest.h"

namespace fat_p::testing::skeletonserializer
{

using fat_p::skeleton::SerializeReader;
using fat_p::skeleton::SerializeWriter;

FATP_TEST_CASE(empty_variable_payloads_roundtrip)
{
    std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
    SerializeWriter writer(stream);
    const std::uint8_t placeholder = 0u;
    writer.writeString("");
    writer.writeBytes(&placeholder, 0u);

    stream.seekg(0);
    SerializeReader reader(stream);
    const std::string actualString = reader.readString();
    const std::vector<std::uint8_t> actualBytes = reader.readBytes();

    FATP_ASSERT_TRUE(actualString.empty(), "An empty string payload must round-trip");
    FATP_ASSERT_TRUE(actualBytes.empty(), "An empty byte payload must round-trip");
    return true;
}

FATP_TEST_CASE(string_roundtrip_across_read_chunks)
{
    std::string expected(10'000, 'x');
    expected[4'500] = '\0';
    expected[9'999] = 'z';

    std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
    SerializeWriter writer(stream);
    writer.writeString(expected);

    stream.seekg(0);
    SerializeReader reader(stream);
    const std::string actual = reader.readString();

    FATP_ASSERT_EQ(actual, expected, "String payload must round-trip across read chunks");
    return true;
}

FATP_TEST_CASE(byte_blob_roundtrip_across_read_chunks)
{
    std::vector<std::uint8_t> expected(10'000);
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        expected[i] = static_cast<std::uint8_t>(i % 251u);
    }

    std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
    SerializeWriter writer(stream);
    writer.writeBytes(expected.data(), expected.size());

    stream.seekg(0);
    SerializeReader reader(stream);
    const std::vector<std::uint8_t> actual = reader.readBytes();

    FATP_ASSERT_EQ(actual.size(), expected.size(), "Byte payload size must round-trip");
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        FATP_ASSERT_EQ(actual[i], expected[i], "Each byte must round-trip across read chunks");
    }
    return true;
}

FATP_TEST_CASE(maximum_declared_string_length_without_payload_is_rejected)
{
    std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
    SerializeWriter writer(stream);
    writer.writeU32(std::numeric_limits<std::uint32_t>::max());

    stream.seekg(0);
    SerializeReader reader(stream);
    FATP_ASSERT_THROWS(reader.readString(),
                       std::runtime_error,
                       "A maximum length prefix without payload bytes must report truncation");
    return true;
}

FATP_TEST_CASE(maximum_declared_byte_length_without_payload_is_rejected)
{
    std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
    SerializeWriter writer(stream);
    writer.writeU32(std::numeric_limits<std::uint32_t>::max());

    stream.seekg(0);
    SerializeReader reader(stream);
    FATP_ASSERT_THROWS(reader.readBytes(),
                       std::runtime_error,
                       "A maximum byte count without payload bytes must report truncation");
    return true;
}

} // namespace fat_p::testing::skeletonserializer

namespace fat_p::testing
{

bool test_SkeletonSerializer()
{
    FATP_PRINT_HEADER(SKELETON SERIALIZER)

    TestRunner runner;
    FATP_RUN_TEST_NS(runner, skeletonserializer, empty_variable_payloads_roundtrip);
    FATP_RUN_TEST_NS(runner, skeletonserializer, string_roundtrip_across_read_chunks);
    FATP_RUN_TEST_NS(runner, skeletonserializer, byte_blob_roundtrip_across_read_chunks);
    FATP_RUN_TEST_NS(runner, skeletonserializer, maximum_declared_string_length_without_payload_is_rejected);
    FATP_RUN_TEST_NS(runner, skeletonserializer, maximum_declared_byte_length_without_payload_is_rejected);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_SkeletonSerializer() ? 0 : 1;
}
#endif
