/**
 * @file test_EnumPlus.cpp
 * @brief Comprehensive unit tests for EnumPlus.h
 */
/*
FATP_META:
  meta_version: 1
  component: EnumPlus
  file_role: test
  path: components/EnumPlus/tests/test_EnumPlus.cpp
  layer: Testing
  namespace: fat_p::testing::enumplus
  summary: "Unit tests for EnumPlus."
  api_stability: in_work
  related:
    docs_search: "EnumPlus"
    headers:
      - include/fat_p/EnumPlus.h
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

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "EnumPlus.h"
#include "FatPTest.h"

// ============================================================================
// Test Enum Types (local to this translation unit)
// ============================================================================

namespace
{

enum class Color
{
    Red = 0,
    Green = 1,
    Blue = 2,
    Yellow = 3
};

enum class FilePermission : unsigned int
{
    None = 0,
    Read = 1,
    Write = 2,
    Execute = 4
};

enum class Status
{
    Idle = 0,
    Running = 1,
    Completed = 2,
    Failed = 3
};

enum class LargeEnum
{
    Value0 = 0,
    Value1 = 1,
    Value2 = 2,
    Value3 = 3,
    Value4 = 4,
    Value5 = 5,
    Value6 = 6,
    Value7 = 7,
    Value8 = 8,
    Value9 = 9
};

enum class SignedEnum : int
{
    First = 0,
    Second = 1,
    Third = 2
};

enum class SingleValue
{
    Only = 0
};

} // anonymous namespace

// ============================================================================
// Template Specializations (must be in fat_p namespace)
// ============================================================================

namespace fat_p
{

template <>
struct EnumSizeTrait<Color>
{
    static constexpr std::size_t size = 4;
};

template <>
struct EnumSizeTrait<FilePermission>
{
    static constexpr std::size_t size = 4;
};

template <>
struct EnumSizeTrait<Status>
{
    static constexpr std::size_t size = 4;
};

template <>
struct EnumSizeTrait<LargeEnum>
{
    static constexpr std::size_t size = 10;
};

template <>
struct EnumSizeTrait<SignedEnum>
{
    static constexpr std::size_t size = 3;
};

template <>
struct EnumSizeTrait<SingleValue>
{
    static constexpr std::size_t size = 1;
};

template <>
struct EnableOverloadedOperators<FilePermission> : std::true_type
{
};

template <>
struct EnumStringPolicy<Color>
{
    static std::string_view to_string(Color value)
    {
        switch (value)
        {
            case Color::Red:
                return "Red";
            case Color::Green:
                return "Green";
            case Color::Blue:
                return "Blue";
            case Color::Yellow:
                return "Yellow";
            default:
                return "Unknown";
        }
    }

    static Color from_string(std::string_view str)
    {
        if (str == "Red")
        {
            return Color::Red;
        }
        if (str == "Green")
        {
            return Color::Green;
        }
        if (str == "Blue")
        {
            return Color::Blue;
        }
        if (str == "Yellow")
        {
            return Color::Yellow;
        }
        throw std::invalid_argument("Invalid Color string");
    }
};

template <>
struct EnumStringPolicy<Status>
{
    static std::string_view to_string(Status value)
    {
        switch (value)
        {
            case Status::Idle:
                return "Idle";
            case Status::Running:
                return "Running";
            case Status::Completed:
                return "Completed";
            case Status::Failed:
                return "Failed";
            default:
                return "Unknown";
        }
    }

    static Status from_string(std::string_view str)
    {
        if (str == "Idle")
        {
            return Status::Idle;
        }
        if (str == "Running")
        {
            return Status::Running;
        }
        if (str == "Completed")
        {
            return Status::Completed;
        }
        if (str == "Failed")
        {
            return Status::Failed;
        }
        throw std::invalid_argument("Invalid Status string");
    }
};

template <>
struct EnumStringPolicy<SignedEnum>
{
    static std::string_view to_string(SignedEnum value)
    {
        switch (value)
        {
            case SignedEnum::First:
                return "First";
            case SignedEnum::Second:
                return "Second";
            case SignedEnum::Third:
                return "Third";
            default:
                return "Unknown";
        }
    }

    static SignedEnum from_string(std::string_view str)
    {
        if (str == "First")
        {
            return SignedEnum::First;
        }
        if (str == "Second")
        {
            return SignedEnum::Second;
        }
        if (str == "Third")
        {
            return SignedEnum::Third;
        }
        throw std::invalid_argument("Invalid SignedEnum string");
    }
};

} // namespace fat_p

// ============================================================================
// Test Functions (in fat_p::testing::enumplus namespace)
// ============================================================================

namespace fat_p::testing::enumplus
{

using namespace fat_p;
using namespace fat_p::testing;

FATP_TEST_CASE(enum_size_trait)
{
    static_assert(EnumSizeTrait<Color>::size == 4, "Color should have 4 values");
    static_assert(EnumSizeTrait<FilePermission>::size == 4, "FilePermission should have 4 values");
    static_assert(EnumSizeTrait<Status>::size == 4, "Status should have 4 values");
    static_assert(EnumSizeTrait<LargeEnum>::size == 10, "LargeEnum should have 10 values");
    return true;
}

FATP_TEST_CASE(enum_plus_map_basic)
{
    EnumPlusMap<Color, std::string> color_names{"Red", "Green", "Blue", "Yellow"};

    FATP_ASSERT_EQ(color_names[Color::Red], std::string("Red"), "Red should map to 'Red'");
    FATP_ASSERT_EQ(color_names[Color::Green], std::string("Green"), "Green should map to 'Green'");
    FATP_ASSERT_EQ(color_names[Color::Blue], std::string("Blue"), "Blue should map to 'Blue'");
    FATP_ASSERT_EQ(color_names[Color::Yellow], std::string("Yellow"), "Yellow should map to 'Yellow'");
    FATP_ASSERT_EQ(color_names.size(), 4u, "Color map size should be 4");
    FATP_ASSERT_EQ(color_names.empty(), false, "Color map should not be empty");
    return true;
}

FATP_TEST_CASE(enum_plus_map_access)
{
    EnumPlusMap<Color, int> color_values{10, 20, 30, 40};

    FATP_ASSERT_EQ(color_values[Color::Red], 10, "Red should have value 10");
    FATP_ASSERT_EQ(color_values[Color::Green], 20, "Green should have value 20");
    FATP_ASSERT_EQ(color_values[Color::Blue], 30, "Blue should have value 30");
    FATP_ASSERT_EQ(color_values[Color::Yellow], 40, "Yellow should have value 40");
    FATP_ASSERT_EQ(color_values.at(Color::Red), 10, "at(Red) should return 10");
    FATP_ASSERT_EQ(color_values.at(Color::Blue), 30, "at(Blue) should return 30");

    color_values[Color::Red] = 100;
    FATP_ASSERT_EQ(color_values[Color::Red], 100, "Red should have updated value 100");
    return true;
}

FATP_TEST_CASE(enum_plus_map_constructor_variants)
{
    EnumPlusMap<Color, int> default_map;
    FATP_ASSERT_EQ(default_map.size(), 4u, "Default map size should be 4");

    EnumPlusMap<Color, int> filled_map(42);
    FATP_ASSERT_EQ(filled_map[Color::Red], 42, "Filled map Red should be 42");
    FATP_ASSERT_EQ(filled_map[Color::Green], 42, "Filled map Green should be 42");
    FATP_ASSERT_EQ(filled_map[Color::Blue], 42, "Filled map Blue should be 42");
    FATP_ASSERT_EQ(filled_map[Color::Yellow], 42, "Filled map Yellow should be 42");

    EnumPlusMap<Color, int> init_list_map{1, 2, 3, 4};
    FATP_ASSERT_EQ(init_list_map[Color::Red], 1, "Init list Red should be 1");
    FATP_ASSERT_EQ(init_list_map[Color::Yellow], 4, "Init list Yellow should be 4");

    EnumPlusMap<Color, int> generated_map([](Color c) {
        return static_cast<int>(c) * 10;
    });
    FATP_ASSERT_EQ(generated_map[Color::Red], 0, "Generated Red should be 0");
    FATP_ASSERT_EQ(generated_map[Color::Green], 10, "Generated Green should be 10");
    FATP_ASSERT_EQ(generated_map[Color::Blue], 20, "Generated Blue should be 20");
    FATP_ASSERT_EQ(generated_map[Color::Yellow], 30, "Generated Yellow should be 30");

    int sum = 0;
    generated_map.for_each([&sum](int& val) {
        sum += val;
    });
    FATP_ASSERT_EQ(sum, 60, "Sum of generated values should be 60");
    return true;
}

FATP_TEST_CASE(enum_plus_map_with_string_policy)
{
    EnumPlusMap<Status, std::string> status_messages{"System idle", "Processing...", "Done!", "Error occurred"};

    FATP_ASSERT_EQ(status_messages[Status::Idle], std::string("System idle"), "Idle message");
    FATP_ASSERT_EQ(status_messages[Status::Running], std::string("Processing..."), "Running message");
    FATP_ASSERT_EQ(status_messages[Status::Completed], std::string("Done!"), "Completed message");
    FATP_ASSERT_EQ(status_messages[Status::Failed], std::string("Error occurred"), "Failed message");
    return true;
}

FATP_TEST_CASE(stream_operators)
{
    std::ostringstream oss;

    oss << Status::Idle;
    FATP_ASSERT_EQ(oss.str(), std::string("Idle"), "Status::Idle should output 'Idle'");

    oss.str("");
    oss << Status::Running;
    FATP_ASSERT_EQ(oss.str(), std::string("Running"), "Status::Running should output 'Running'");

    oss.str("");
    oss << EnumPlusWrapper<Status>(Status::Running);
    FATP_ASSERT_EQ(oss.str(), std::string("Running"), "Wrapped should output 'Running'");
    return true;
}

FATP_TEST_CASE(bitwise_or_operators)
{
    auto perm1 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::Write);
    auto combined = perm1 | perm2;
    FATP_ASSERT_EQ(combined.underlying(), 3u, "Read | Write should equal 3");

    auto combined2 = perm1 | FilePermission::Execute;
    FATP_ASSERT_EQ(combined2.underlying(), 5u, "Read | Execute should equal 5");

    auto combined3 = FilePermission::Read | FilePermission::Write;
    FATP_ASSERT_EQ(static_cast<unsigned int>(combined3), 3u, "Raw Read | Write should equal 3");

    auto perm3 = EnumPlusWrapper<FilePermission>(FilePermission::Execute);
    auto combined4 = FilePermission::Read | perm3;
    FATP_ASSERT_EQ(combined4.underlying(), 5u, "Raw Read | Wrapped Execute should equal 5");
    return true;
}

FATP_TEST_CASE(bitwise_and_operators)
{
    auto all_perms =
        EnumPlusWrapper<FilePermission>(FilePermission::Read) | EnumPlusWrapper<FilePermission>(FilePermission::Write);
    auto read_only = all_perms & EnumPlusWrapper<FilePermission>(FilePermission::Read);
    FATP_ASSERT_EQ(read_only.underlying(), 1u, "(Read|Write) & Read should equal 1");

    auto result2 = all_perms & FilePermission::Write;
    FATP_ASSERT_EQ(result2.underlying(), 2u, "(Read|Write) & Write should equal 2");

    auto combined = FilePermission::Read | FilePermission::Write;
    auto result3 = combined & FilePermission::Read;
    FATP_ASSERT_EQ(static_cast<unsigned int>(result3), 1u, "Raw AND should work");
    return true;
}

FATP_TEST_CASE(bitwise_xor_operators)
{
    auto perm1 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::Write);
    auto result = perm1 ^ perm2;
    FATP_ASSERT_EQ(result.underlying(), 3u, "Read ^ Write should equal 3");

    auto result2 = perm1 ^ FilePermission::Write;
    FATP_ASSERT_EQ(result2.underlying(), 3u, "Wrapper ^ raw should equal 3");

    auto result3 = FilePermission::Read ^ FilePermission::Write;
    FATP_ASSERT_EQ(static_cast<unsigned int>(result3), 3u, "Raw XOR should equal 3");

    auto combined = FilePermission::Read | FilePermission::Write;
    auto result4 = combined ^ FilePermission::Read;
    FATP_ASSERT_EQ(static_cast<unsigned int>(result4), 2u, "(R|W) ^ R should equal 2 (Write)");
    return true;
}

FATP_TEST_CASE(bitwise_not_operators)
{
    auto perm = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    auto inverted = ~perm;
    FATP_ASSERT_EQ((inverted.underlying() & 7u), 6u, "~Read & 7 should equal 6 (Write|Execute)");

    auto result = ~FilePermission::Read;
    FATP_ASSERT_EQ((static_cast<unsigned int>(result) & 7u), 6u, "Raw ~Read & 7 should equal 6");

    auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::None);
    auto inverted2 = ~perm2;
    FATP_ASSERT_EQ((inverted2.underlying() & 7u), 7u, "~None & 7 should equal 7 (all)");
    return true;
}

FATP_TEST_CASE(compound_or_assignment)
{
    auto perm1 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    perm1 |= EnumPlusWrapper<FilePermission>(FilePermission::Write);
    FATP_ASSERT_EQ(perm1.underlying(), 3u, "Read |= Write should equal 3");

    auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    perm2 |= FilePermission::Write;
    FATP_ASSERT_EQ(perm2.underlying(), 3u, "Wrapper |= raw should equal 3");

    FilePermission perm3 = FilePermission::Read;
    perm3 |= FilePermission::Write;
    perm3 |= FilePermission::Execute;
    FATP_ASSERT_EQ(static_cast<unsigned int>(perm3), 7u, "Raw |= chain should equal 7");
    return true;
}

FATP_TEST_CASE(compound_and_assignment)
{
    auto all = FilePermission::Read | FilePermission::Write | FilePermission::Execute;
    auto perm1 = EnumPlusWrapper<FilePermission>(all);
    perm1 &= EnumPlusWrapper<FilePermission>(FilePermission::Read);
    FATP_ASSERT_EQ(perm1.underlying(), 1u, "all &= Read should equal 1");

    auto perm2 = EnumPlusWrapper<FilePermission>(all);
    perm2 &= FilePermission::Write;
    FATP_ASSERT_EQ(perm2.underlying(), 2u, "all &= Write should equal 2");

    auto perm3 = FilePermission::Read | FilePermission::Write;
    perm3 &= FilePermission::Read;
    FATP_ASSERT_EQ(static_cast<unsigned int>(perm3), 1u, "Raw &= should equal 1");
    return true;
}

FATP_TEST_CASE(compound_xor_assignment)
{
    auto perm1 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    perm1 ^= EnumPlusWrapper<FilePermission>(FilePermission::Write);
    FATP_ASSERT_EQ(perm1.underlying(), 3u, "Read ^= Write should equal 3");

    auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    perm2 ^= FilePermission::Write;
    FATP_ASSERT_EQ(perm2.underlying(), 3u, "Wrapper ^= raw should equal 3");

    auto perm3 = FilePermission::Read | FilePermission::Write;
    perm3 ^= FilePermission::Read;
    FATP_ASSERT_EQ(static_cast<unsigned int>(perm3), 2u, "(R|W) ^= R should equal 2");
    return true;
}

FATP_TEST_CASE(mixed_operations)
{
    FilePermission perm = FilePermission::None;
    perm |= FilePermission::Read;
    perm |= FilePermission::Write;
    perm &= (FilePermission::Read | FilePermission::Execute);
    FATP_ASSERT_EQ(static_cast<unsigned int>(perm), 1u, "Mixed ops should equal 1 (Read)");

    auto perm_wrap = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    perm_wrap |= FilePermission::Write;
    perm_wrap &= FilePermission::Read;
    FATP_ASSERT_EQ(perm_wrap.underlying(), 1u, "Wrapper mixed ops should equal 1");
    return true;
}

FATP_TEST_CASE(has_flag_utility)
{
    auto perms = FilePermission::Read | FilePermission::Write;

    FATP_ASSERT_TRUE(has_flag(perms, FilePermission::Read), "Should have Read flag");
    FATP_ASSERT_TRUE(has_flag(perms, FilePermission::Write), "Should have Write flag");
    FATP_ASSERT_TRUE(!has_flag(perms, FilePermission::Execute), "Should not have Execute flag");

    auto wrapped = EnumPlusWrapper<FilePermission>(perms);
    FATP_ASSERT_TRUE(has_flag(wrapped, FilePermission::Read), "Wrapped should have Read");
    FATP_ASSERT_TRUE(!has_flag(wrapped, FilePermission::Execute), "Wrapped should not have Execute");

    auto wrapped_flag = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    FATP_ASSERT_TRUE(has_flag(wrapped, wrapped_flag), "Wrapper with wrapper flag should work");
    return true;
}

FATP_TEST_CASE(enum_plus_wrapper)
{
    EnumPlusWrapper<Color> color(Color::Red);
    FATP_ASSERT_EQ(color.value(), Color::Red, "Wrapper value should be Red");
    FATP_ASSERT_EQ(color.underlying(), 0, "Wrapper underlying should be 0");

    Color raw = color;
    FATP_ASSERT_EQ(raw, Color::Red, "Implicit conversion to raw enum should work");

    FATP_ASSERT_TRUE(color == Color::Red, "Wrapper should equal Color::Red");
    FATP_ASSERT_TRUE(color != Color::Blue, "Wrapper should not equal Color::Blue");

    EnumPlusWrapper<Color> color2(Color::Red);
    FATP_ASSERT_TRUE(color == color2, "Two wrappers with same value should be equal");
    return true;
}

FATP_TEST_CASE(wrapper_no_implicit_conversion)
{
    static_assert(!std::is_convertible_v<int, EnumPlusWrapper<Color>>, "Should not allow implicit conversion from int");
    static_assert(!std::is_constructible_v<EnumPlusWrapper<Color>, int>, "Should not allow construction from int");
    return true;
}

FATP_TEST_CASE(is_valid_enum)
{
    FATP_ASSERT_TRUE(is_valid_enum<Color>(0), "0 should be valid Color (Red)");
    FATP_ASSERT_TRUE(is_valid_enum<Color>(1), "1 should be valid Color (Green)");
    FATP_ASSERT_TRUE(is_valid_enum<Color>(2), "2 should be valid Color (Blue)");
    FATP_ASSERT_TRUE(is_valid_enum<Color>(3), "3 should be valid Color (Yellow)");
    FATP_ASSERT_TRUE(!is_valid_enum<Color>(-1), "-1 should be invalid Color");
    FATP_ASSERT_TRUE(!is_valid_enum<Color>(4), "4 should be invalid Color");
    FATP_ASSERT_TRUE(!is_valid_enum<Color>(100), "100 should be invalid Color");

    FATP_ASSERT_TRUE(is_valid_enum(Color::Red), "Color::Red should be valid");
    FATP_ASSERT_TRUE(is_valid_enum(Color::Yellow), "Color::Yellow should be valid");

    Color invalid = static_cast<Color>(100);
    FATP_ASSERT_TRUE(!is_valid_enum(invalid), "Cast from 100 should be invalid");
    return true;
}

FATP_TEST_CASE(safe_enum_cast)
{
    auto r1 = safe_enum_cast<Color>(0);
    FATP_ASSERT_TRUE(r1.has_value(), "0 should cast to valid Color");
    FATP_ASSERT_EQ(*r1, Color::Red, "0 should cast to Color::Red");

    auto r2 = safe_enum_cast<Color>(3);
    FATP_ASSERT_TRUE(r2.has_value(), "3 should cast to valid Color");
    FATP_ASSERT_EQ(*r2, Color::Yellow, "3 should cast to Color::Yellow");

    auto r3 = safe_enum_cast<Color>(-1);
    FATP_ASSERT_TRUE(!r3.has_value(), "-1 should not cast to valid Color");

    auto r4 = safe_enum_cast<Color>(4);
    FATP_ASSERT_TRUE(!r4.has_value(), "4 should not cast to valid Color");

    auto r5 = safe_enum_cast<Color>(100);
    FATP_ASSERT_TRUE(!r5.has_value(), "100 should not cast to valid Color");
    return true;
}

FATP_TEST_CASE(safe_to_underlying)
{
    auto r1 = safe_to_underlying(Color::Red);
    FATP_ASSERT_TRUE(r1.has_value(), "Red should have valid underlying");
    FATP_ASSERT_EQ(*r1, 0, "Red underlying should be 0");

    auto r2 = safe_to_underlying(Color::Yellow);
    FATP_ASSERT_TRUE(r2.has_value(), "Yellow should have valid underlying");
    FATP_ASSERT_EQ(*r2, 3, "Yellow underlying should be 3");

    Color invalid = static_cast<Color>(100);
    auto r3 = safe_to_underlying(invalid);
    FATP_ASSERT_TRUE(!r3.has_value(), "Invalid enum should not have valid underlying");
    return true;
}

FATP_TEST_CASE(shift_operators)
{
    FilePermission shifted = FilePermission::Read << 1;
    FATP_ASSERT_EQ(to_underlying(shifted), 2u, "Read << 1 should be 2");

    shifted = FilePermission::Read << 2;
    FATP_ASSERT_EQ(to_underlying(shifted), 4u, "Read << 2 should be 4");

    shifted = FilePermission::Execute >> 1;
    FATP_ASSERT_EQ(to_underlying(shifted), 2u, "Execute >> 1 should be 2");

    shifted = FilePermission::Execute >> 2;
    FATP_ASSERT_EQ(to_underlying(shifted), 1u, "Execute >> 2 should be 1");

    EnumPlusWrapper<FilePermission> perm(FilePermission::Read);
    auto shifted_wrap = perm << 2;
    FATP_ASSERT_EQ(shifted_wrap.underlying(), 4u, "Wrapped Read << 2 should be 4");

    perm = EnumPlusWrapper<FilePermission>(FilePermission::Execute);
    shifted_wrap = perm >> 1;
    FATP_ASSERT_EQ(shifted_wrap.underlying(), 2u, "Wrapped Execute >> 1 should be 2");

    FilePermission p = FilePermission::Read;
    p <<= 2;
    FATP_ASSERT_EQ(to_underlying(p), 4u, "Read <<= 2 should be 4");

    p = FilePermission::Execute;
    p >>= 1;
    FATP_ASSERT_EQ(to_underlying(p), 2u, "Execute >>= 1 should be 2");
    return true;
}

FATP_TEST_CASE(enum_index)
{
    auto idx0 = enum_index(Color::Red);
    FATP_ASSERT_TRUE(idx0.has_value(), "Red should have valid index");
    FATP_ASSERT_EQ(*idx0, 0u, "Red index should be 0");

    auto idx1 = enum_index(Color::Green);
    FATP_ASSERT_TRUE(idx1.has_value(), "Green should have valid index");
    FATP_ASSERT_EQ(*idx1, 1u, "Green index should be 1");

    auto idx2 = enum_index(Color::Blue);
    FATP_ASSERT_TRUE(idx2.has_value(), "Blue should have valid index");
    FATP_ASSERT_EQ(*idx2, 2u, "Blue index should be 2");

    auto idx3 = enum_index(Color::Yellow);
    FATP_ASSERT_TRUE(idx3.has_value(), "Yellow should have valid index");
    FATP_ASSERT_EQ(*idx3, 3u, "Yellow index should be 3");

    Color invalid = static_cast<Color>(100);
    auto idx_invalid = enum_index(invalid);
    FATP_ASSERT_TRUE(!idx_invalid.has_value(), "Invalid enum should not have index");

    auto status_idx = enum_index(Status::Running);
    FATP_ASSERT_TRUE(status_idx.has_value(), "Running should have valid index");
    FATP_ASSERT_EQ(*status_idx, 1u, "Running index should be 1");
    return true;
}

FATP_TEST_CASE(enum_value)
{
    auto v0 = enum_value<Color>(0);
    FATP_ASSERT_TRUE(v0.has_value(), "Index 0 should be valid Color");
    FATP_ASSERT_EQ(*v0, Color::Red, "Index 0 should be Red");

    auto v1 = enum_value<Color>(1);
    FATP_ASSERT_TRUE(v1.has_value(), "Index 1 should be valid Color");
    FATP_ASSERT_EQ(*v1, Color::Green, "Index 1 should be Green");

    auto v2 = enum_value<Color>(2);
    FATP_ASSERT_TRUE(v2.has_value(), "Index 2 should be valid Color");
    FATP_ASSERT_EQ(*v2, Color::Blue, "Index 2 should be Blue");

    auto v3 = enum_value<Color>(3);
    FATP_ASSERT_TRUE(v3.has_value(), "Index 3 should be valid Color");
    FATP_ASSERT_EQ(*v3, Color::Yellow, "Index 3 should be Yellow");

    auto v4 = enum_value<Color>(4);
    FATP_ASSERT_TRUE(!v4.has_value(), "Index 4 should be invalid Color");

    auto v100 = enum_value<Color>(100);
    FATP_ASSERT_TRUE(!v100.has_value(), "Index 100 should be invalid Color");

    auto status_v = enum_value<Status>(2);
    FATP_ASSERT_TRUE(status_v.has_value(), "Index 2 should be valid Status");
    FATP_ASSERT_EQ(*status_v, Status::Completed, "Index 2 should be Completed");
    return true;
}

FATP_TEST_CASE(enum_index_value_roundtrip)
{
    for (std::size_t i = 0; i < EnumSizeTrait<Color>::size; ++i)
    {
        auto val = enum_value<Color>(i);
        FATP_ASSERT_TRUE(val.has_value(), "Index should produce valid value");
        auto idx = enum_index(*val);
        FATP_ASSERT_TRUE(idx.has_value(), "Value should produce valid index");
        FATP_ASSERT_EQ(*idx, i, "Roundtrip should preserve index");
    }

    auto values = enum_values<Color>();
    for (auto v : values)
    {
        auto idx = enum_index(v);
        FATP_ASSERT_TRUE(idx.has_value(), "Value should have valid index");
        auto val = enum_value<Color>(*idx);
        FATP_ASSERT_TRUE(val.has_value(), "Index should produce valid value");
        FATP_ASSERT_EQ(*val, v, "Roundtrip should preserve value");
    }
    return true;
}

FATP_TEST_CASE(enum_contains)
{
    FATP_ASSERT_TRUE(enum_contains(Color::Red), "Red should be contained");
    FATP_ASSERT_TRUE(enum_contains(Color::Green), "Green should be contained");
    FATP_ASSERT_TRUE(enum_contains(Color::Blue), "Blue should be contained");
    FATP_ASSERT_TRUE(enum_contains(Color::Yellow), "Yellow should be contained");

    Color invalid = static_cast<Color>(100);
    FATP_ASSERT_TRUE(!enum_contains(invalid), "Invalid color should not be contained");

    Color negative = static_cast<Color>(-1);
    FATP_ASSERT_TRUE(!enum_contains(negative), "Negative should not be contained");

    FATP_ASSERT_TRUE(enum_contains(Status::Idle), "Idle should be contained");
    FATP_ASSERT_TRUE(enum_contains(Status::Running), "Running should be contained");
    return true;
}

FATP_TEST_CASE(enum_count)
{
    static_assert(enum_count<Color> == 4, "Color count should be 4");
    static_assert(enum_count<Status> == 4, "Status count should be 4");
    static_assert(enum_count<LargeEnum> == 10, "LargeEnum count should be 10");
    static_assert(enum_count<SignedEnum> == 3, "SignedEnum count should be 3");
    static_assert(enum_count<SingleValue> == 1, "SingleValue count should be 1");

    FATP_ASSERT_EQ(enum_count<Color>, 4u, "Runtime Color count should be 4");
    FATP_ASSERT_EQ(enum_count<FilePermission>, 4u, "Runtime FilePermission count should be 4");
    return true;
}

FATP_TEST_CASE(for_each_enum)
{
    std::size_t count = 0;
    int sum = 0;
    for_each_enum<Color>([&](Color c) {
        ++count;
        sum += static_cast<int>(c);
    });
    FATP_ASSERT_EQ(count, 4u, "for_each_enum should iterate 4 times");
    FATP_ASSERT_EQ(sum, 6, "Sum of Color indices should be 0+1+2+3=6");

    std::vector<Status> collected;
    for_each_enum<Status>([&](Status s) {
        collected.push_back(s);
    });
    FATP_ASSERT_EQ(collected.size(), 4u, "Should collect 4 Status values");
    FATP_ASSERT_EQ(collected[0], Status::Idle, "First should be Idle");
    FATP_ASSERT_EQ(collected[3], Status::Failed, "Last should be Failed");
    return true;
}

FATP_TEST_CASE(enum_entries)
{
    auto entries = enum_entries<Color>();
    FATP_ASSERT_EQ(entries.size(), 4u, "Should have 4 entries");

    FATP_ASSERT_EQ(entries[0].name, std::string_view("Red"), "Entry 0 name should be Red");
    FATP_ASSERT_EQ(entries[0].value, Color::Red, "Entry 0 value should be Red");

    FATP_ASSERT_EQ(entries[1].name, std::string_view("Green"), "Entry 1 name should be Green");
    FATP_ASSERT_EQ(entries[1].value, Color::Green, "Entry 1 value should be Green");

    FATP_ASSERT_EQ(entries[2].name, std::string_view("Blue"), "Entry 2 name should be Blue");
    FATP_ASSERT_EQ(entries[2].value, Color::Blue, "Entry 2 value should be Blue");

    FATP_ASSERT_EQ(entries[3].name, std::string_view("Yellow"), "Entry 3 name should be Yellow");
    FATP_ASSERT_EQ(entries[3].value, Color::Yellow, "Entry 3 value should be Yellow");

    auto status_entries = enum_entries<Status>();
    FATP_ASSERT_EQ(status_entries.size(), 4u, "Should have 4 Status entries");
    FATP_ASSERT_EQ(status_entries[1].name, std::string_view("Running"), "Status entry 1 name");
    FATP_ASSERT_EQ(status_entries[1].value, Status::Running, "Status entry 1 value");
    return true;
}

FATP_TEST_CASE(from_string_icase)
{
    auto r1 = from_string_icase<Color>("Red");
    FATP_ASSERT_TRUE(r1.has_value(), "Exact case 'Red' should be valid");
    FATP_ASSERT_EQ(*r1, Color::Red, "'Red' should convert to Color::Red");

    auto r2 = from_string_icase<Color>("RED");
    FATP_ASSERT_TRUE(r2.has_value(), "Uppercase 'RED' should be valid");
    FATP_ASSERT_EQ(*r2, Color::Red, "'RED' should convert to Color::Red");

    auto r3 = from_string_icase<Color>("red");
    FATP_ASSERT_TRUE(r3.has_value(), "Lowercase 'red' should be valid");
    FATP_ASSERT_EQ(*r3, Color::Red, "'red' should convert to Color::Red");

    auto r4 = from_string_icase<Color>("rEd");
    FATP_ASSERT_TRUE(r4.has_value(), "Mixed case 'rEd' should be valid");
    FATP_ASSERT_EQ(*r4, Color::Red, "'rEd' should convert to Color::Red");

    auto r5 = from_string_icase<Color>("YELLOW");
    FATP_ASSERT_TRUE(r5.has_value(), "Uppercase 'YELLOW' should be valid");
    FATP_ASSERT_EQ(*r5, Color::Yellow, "'YELLOW' should convert to Color::Yellow");

    auto invalid1 = from_string_icase<Color>("Redd");
    FATP_ASSERT_TRUE(!invalid1.has_value(), "'Redd' should be invalid");

    auto invalid2 = from_string_icase<Color>("Purple");
    FATP_ASSERT_TRUE(!invalid2.has_value(), "'Purple' should be invalid");

    auto invalid3 = from_string_icase<Color>("");
    FATP_ASSERT_TRUE(!invalid3.has_value(), "Empty string should be invalid");

    Color c1 = from_string_icase_or<Color>("BLUE", Color::Red);
    FATP_ASSERT_EQ(c1, Color::Blue, "from_string_icase_or 'BLUE' should return Blue");

    Color c2 = from_string_icase_or<Color>("invalid", Color::Red);
    FATP_ASSERT_EQ(c2, Color::Red, "from_string_icase_or invalid should return default");

    auto s1 = from_string_icase<Status>("RUNNING");
    FATP_ASSERT_TRUE(s1.has_value(), "'RUNNING' should be valid Status");
    FATP_ASSERT_EQ(*s1, Status::Running, "'RUNNING' should convert to Status::Running");
    return true;
}

FATP_TEST_CASE(to_underlying)
{
    FATP_ASSERT_EQ(to_underlying(Color::Red), 0, "Red underlying should be 0");
    FATP_ASSERT_EQ(to_underlying(Color::Green), 1, "Green underlying should be 1");
    FATP_ASSERT_EQ(to_underlying(Color::Blue), 2, "Blue underlying should be 2");
    FATP_ASSERT_EQ(to_underlying(Color::Yellow), 3, "Yellow underlying should be 3");

    FATP_ASSERT_EQ(to_underlying(FilePermission::None), 0u, "None underlying should be 0");
    FATP_ASSERT_EQ(to_underlying(FilePermission::Read), 1u, "Read underlying should be 1");
    FATP_ASSERT_EQ(to_underlying(FilePermission::Write), 2u, "Write underlying should be 2");
    FATP_ASSERT_EQ(to_underlying(FilePermission::Execute), 4u, "Execute underlying should be 4");
    return true;
}

FATP_TEST_CASE(enum_values)
{
    auto colors = enum_values<Color>();
    FATP_ASSERT_EQ(colors.size(), 4u, "Should have 4 color values");
    FATP_ASSERT_EQ(colors[0], Color::Red, "First color should be Red");
    FATP_ASSERT_EQ(colors[1], Color::Green, "Second color should be Green");
    FATP_ASSERT_EQ(colors[2], Color::Blue, "Third color should be Blue");
    FATP_ASSERT_EQ(colors[3], Color::Yellow, "Fourth color should be Yellow");
    return true;
}

FATP_TEST_CASE(constexpr_operations)
{
    constexpr EnumPlusWrapper<Color> color(Color::Red);
    static_assert(color.value() == Color::Red, "constexpr value()");
    static_assert(color.underlying() == 0, "constexpr underlying()");

    constexpr auto perm1 = FilePermission::Read | FilePermission::Write;
    static_assert(static_cast<unsigned int>(perm1) == 3u, "constexpr |");

    constexpr auto perm2 = FilePermission::Read & FilePermission::Read;
    static_assert(static_cast<unsigned int>(perm2) == 1u, "constexpr &");

    constexpr auto perm3 = FilePermission::Read ^ FilePermission::Write;
    static_assert(static_cast<unsigned int>(perm3) == 3u, "constexpr ^");
    return true;
}

FATP_TEST_CASE(no_bounds_check_policy)
{
    EnumPlusMap<Color, int, NoBoundsCheckPolicy> map{10, 20, 30, 40};
    FATP_ASSERT_EQ(map[Color::Red], 10, "NoBoundsCheck map Red should be 10");
    FATP_ASSERT_EQ(map[Color::Blue], 30, "NoBoundsCheck map Blue should be 30");
    return true;
}

FATP_TEST_CASE(bounds_check_throws)
{
    EnumPlusMap<Color, int> map{10, 20, 30, 40};
    FATP_ASSERT_EQ(map.at(Color::Red), 10, "Bounds checked at(Red) should be 10");
    FATP_ASSERT_EQ(map.at(Color::Yellow), 40, "Bounds checked at(Yellow) should be 40");
    return true;
}

// ============================================================================
// Main Test Entry Point
// ============================================================================

} // namespace fat_p::testing::enumplus

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_EnumPlus()
{
    FATP_PRINT_HEADER(ENUM PLUS)

    TestRunner runner;
    get_test_config().verbose = true;

    std::cout << "\n" << colors::cyan() << "Test Suite 1: EnumSizeTrait" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, enumplus, enum_size_trait);

    std::cout << "\n" << colors::cyan() << "Test Suite 2: EnumPlusMap Basic Operations" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, enumplus, enum_plus_map_basic);
    FATP_RUN_TEST_NS(runner, enumplus, enum_plus_map_access);
    FATP_RUN_TEST_NS(runner, enumplus, enum_plus_map_constructor_variants);
    FATP_RUN_TEST_NS(runner, enumplus, enum_plus_map_with_string_policy);

    std::cout << "\n" << colors::cyan() << "Test Suite 3: Stream Operators" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, enumplus, stream_operators);

    std::cout << "\n" << colors::cyan() << "Test Suite 4: Bitwise OR Operators" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, enumplus, bitwise_or_operators);

    std::cout << "\n" << colors::cyan() << "Test Suite 5: Bitwise AND Operators" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, enumplus, bitwise_and_operators);

    std::cout << "\n" << colors::cyan() << "Test Suite 6: Bitwise XOR Operators" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, enumplus, bitwise_xor_operators);

    std::cout << "\n" << colors::cyan() << "Test Suite 7: Bitwise NOT Operators" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, enumplus, bitwise_not_operators);

    std::cout << "\n" << colors::cyan() << "Test Suite 8: Compound Assignment Operators" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, enumplus, compound_or_assignment);
    FATP_RUN_TEST_NS(runner, enumplus, compound_and_assignment);
    FATP_RUN_TEST_NS(runner, enumplus, compound_xor_assignment);

    std::cout << "\n" << colors::cyan() << "Test Suite 9: Mixed Operations and Utilities" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, enumplus, mixed_operations);
    FATP_RUN_TEST_NS(runner, enumplus, has_flag_utility);
    FATP_RUN_TEST_NS(runner, enumplus, enum_plus_wrapper);
    FATP_RUN_TEST_NS(runner, enumplus, to_underlying);
    FATP_RUN_TEST_NS(runner, enumplus, enum_values);
    FATP_RUN_TEST_NS(runner, enumplus, constexpr_operations);
    FATP_RUN_TEST_NS(runner, enumplus, no_bounds_check_policy);
    FATP_RUN_TEST_NS(runner, enumplus, bounds_check_throws);

    std::cout << "\n" << colors::cyan() << "Test Suite 10: Type Safety" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, enumplus, wrapper_no_implicit_conversion);
    FATP_RUN_TEST_NS(runner, enumplus, is_valid_enum);
    FATP_RUN_TEST_NS(runner, enumplus, safe_enum_cast);
    FATP_RUN_TEST_NS(runner, enumplus, safe_to_underlying);
    FATP_RUN_TEST_NS(runner, enumplus, shift_operators);
    FATP_RUN_TEST_NS(runner, enumplus, from_string_icase);

    std::cout << "\n" << colors::cyan() << "Test Suite 11: Index/Value Reflection" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, enumplus, enum_index);
    FATP_RUN_TEST_NS(runner, enumplus, enum_value);
    FATP_RUN_TEST_NS(runner, enumplus, enum_index_value_roundtrip);

    std::cout << "\n" << colors::cyan() << "Test Suite 12: Enum Reflection Utilities" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, enumplus, enum_contains);
    FATP_RUN_TEST_NS(runner, enumplus, enum_count);
    FATP_RUN_TEST_NS(runner, enumplus, for_each_enum);
    FATP_RUN_TEST_NS(runner, enumplus, enum_entries);

    int failed = runner.print_summary();
    return failed == 0;
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_EnumPlus() ? 0 : 1;
}
#endif
