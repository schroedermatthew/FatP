#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "EnumPlus.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_EnumPlus.h"
#endif

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

template<>
struct EnumSizeTrait<Color>
{
    static constexpr std::size_t size = 4;
};

template<>
struct EnumSizeTrait<FilePermission>
{
    static constexpr std::size_t size = 4;
};

template<>
struct EnumSizeTrait<Status>
{
    static constexpr std::size_t size = 4;
};

template<>
struct EnumSizeTrait<LargeEnum>
{
    static constexpr std::size_t size = 10;
};

template<>
struct EnumSizeTrait<SignedEnum>
{
    static constexpr std::size_t size = 3;
};

template<>
struct EnumSizeTrait<SingleValue>
{
    static constexpr std::size_t size = 1;
};

template<>
struct EnableOverloadedOperators<FilePermission> : std::true_type
{
};

template<>
struct EnumStringPolicy<Color>
{
    static constexpr bool has_names = true;

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

template<>
struct EnumStringPolicy<Status>
{
    static constexpr bool has_names = true;

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

template<>
struct EnumStringPolicy<SignedEnum>
{
    static constexpr bool has_names = true;

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
// Test Functions (anonymous namespace)
// ============================================================================

namespace
{

using namespace fat_p;
using namespace fat_p::testing;

bool test_enum_size_trait()
{
    static_assert(EnumSizeTrait<Color>::size == 4, "Color should have 4 values");
    static_assert(EnumSizeTrait<FilePermission>::size == 4, "FilePermission should have 4 values");
    static_assert(EnumSizeTrait<Status>::size == 4, "Status should have 4 values");
    static_assert(EnumSizeTrait<LargeEnum>::size == 10, "LargeEnum should have 10 values");
    return true;
}

bool test_enum_plus_map_basic()
{
    EnumPlusMap<Color, std::string> color_names{"Red", "Green", "Blue", "Yellow"};

    ASSERT_EQ(color_names[Color::Red], std::string("Red"), "Red should map to 'Red'");
    ASSERT_EQ(color_names[Color::Green], std::string("Green"), "Green should map to 'Green'");
    ASSERT_EQ(color_names[Color::Blue], std::string("Blue"), "Blue should map to 'Blue'");
    ASSERT_EQ(color_names[Color::Yellow], std::string("Yellow"), "Yellow should map to 'Yellow'");
    ASSERT_EQ(color_names.size(), 4u, "Color map size should be 4");
    ASSERT_EQ(color_names.empty(), false, "Color map should not be empty");
    return true;
}

bool test_enum_plus_map_access()
{
    EnumPlusMap<Color, int> color_values{10, 20, 30, 40};

    ASSERT_EQ(color_values[Color::Red], 10, "Red should have value 10");
    ASSERT_EQ(color_values[Color::Green], 20, "Green should have value 20");
    ASSERT_EQ(color_values[Color::Blue], 30, "Blue should have value 30");
    ASSERT_EQ(color_values[Color::Yellow], 40, "Yellow should have value 40");
    ASSERT_EQ(color_values.at(Color::Red), 10, "at(Red) should return 10");
    ASSERT_EQ(color_values.at(Color::Blue), 30, "at(Blue) should return 30");

    color_values[Color::Red] = 100;
    ASSERT_EQ(color_values[Color::Red], 100, "Red should have updated value 100");
    return true;
}

bool test_enum_plus_map_constructor_variants()
{
    EnumPlusMap<Color, int> default_map;
    ASSERT_EQ(default_map.size(), 4u, "Default map size should be 4");

    EnumPlusMap<Color, int> filled_map(42);
    ASSERT_EQ(filled_map[Color::Red], 42, "Filled map Red should be 42");
    ASSERT_EQ(filled_map[Color::Green], 42, "Filled map Green should be 42");
    ASSERT_EQ(filled_map[Color::Blue], 42, "Filled map Blue should be 42");
    ASSERT_EQ(filled_map[Color::Yellow], 42, "Filled map Yellow should be 42");

    EnumPlusMap<Color, int> init_list_map{1, 2, 3, 4};
    ASSERT_EQ(init_list_map[Color::Red], 1, "Init list Red should be 1");
    ASSERT_EQ(init_list_map[Color::Yellow], 4, "Init list Yellow should be 4");

    EnumPlusMap<Color, int> generated_map([](Color c) { return static_cast<int>(c) * 10; });
    ASSERT_EQ(generated_map[Color::Red], 0, "Generated Red should be 0");
    ASSERT_EQ(generated_map[Color::Green], 10, "Generated Green should be 10");
    ASSERT_EQ(generated_map[Color::Blue], 20, "Generated Blue should be 20");
    ASSERT_EQ(generated_map[Color::Yellow], 30, "Generated Yellow should be 30");

    int sum = 0;
    generated_map.for_each([&sum](int& val) { sum += val; });
    ASSERT_EQ(sum, 60, "Sum of generated values should be 60");
    return true;
}

bool test_enum_plus_map_with_string_policy()
{
    EnumPlusMap<Status, std::string> status_messages{
        "System idle", "Processing...", "Done!", "Error occurred"};

    ASSERT_EQ(status_messages[Status::Idle], std::string("System idle"), "Idle message");
    ASSERT_EQ(status_messages[Status::Running], std::string("Processing..."), "Running message");
    ASSERT_EQ(status_messages[Status::Completed], std::string("Done!"), "Completed message");
    ASSERT_EQ(status_messages[Status::Failed], std::string("Error occurred"), "Failed message");
    return true;
}

bool test_stream_operators()
{
    std::ostringstream oss;

    oss << Status::Idle;
    ASSERT_EQ(oss.str(), std::string("Idle"), "Status::Idle should output 'Idle'");

    oss.str("");
    oss << Status::Running;
    ASSERT_EQ(oss.str(), std::string("Running"), "Status::Running should output 'Running'");

    oss.str("");
    oss << EnumPlusWrapper<Status>(Status::Running);
    ASSERT_EQ(oss.str(), std::string("Running"), "Wrapped should output 'Running'");
    return true;
}

bool test_bitwise_or_operators()
{
    auto perm1 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::Write);
    auto combined = perm1 | perm2;
    ASSERT_EQ(combined.underlying(), 3u, "Read | Write should equal 3");

    auto combined2 = perm1 | FilePermission::Execute;
    ASSERT_EQ(combined2.underlying(), 5u, "Read | Execute should equal 5");

    auto combined3 = FilePermission::Read | FilePermission::Write;
    ASSERT_EQ(static_cast<unsigned int>(combined3), 3u, "Raw Read | Write should equal 3");

    auto perm3 = EnumPlusWrapper<FilePermission>(FilePermission::Execute);
    auto combined4 = FilePermission::Read | perm3;
    ASSERT_EQ(combined4.underlying(), 5u, "Raw Read | Wrapped Execute should equal 5");
    return true;
}

bool test_bitwise_and_operators()
{
    auto all_perms = EnumPlusWrapper<FilePermission>(FilePermission::Read) |
                     EnumPlusWrapper<FilePermission>(FilePermission::Write);
    auto read_only = all_perms & EnumPlusWrapper<FilePermission>(FilePermission::Read);
    ASSERT_EQ(read_only.underlying(), 1u, "(Read|Write) & Read should equal 1");

    auto result2 = all_perms & FilePermission::Write;
    ASSERT_EQ(result2.underlying(), 2u, "(Read|Write) & Write should equal 2");

    auto combined = FilePermission::Read | FilePermission::Write;
    auto result3 = combined & FilePermission::Read;
    ASSERT_EQ(static_cast<unsigned int>(result3), 1u, "Raw AND should work");
    return true;
}

bool test_bitwise_xor_operators()
{
    auto perm1 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::Write);
    auto result = perm1 ^ perm2;
    ASSERT_EQ(result.underlying(), 3u, "Read ^ Write should equal 3");

    auto result2 = perm1 ^ FilePermission::Write;
    ASSERT_EQ(result2.underlying(), 3u, "Wrapper ^ raw should equal 3");

    auto result3 = FilePermission::Read ^ FilePermission::Write;
    ASSERT_EQ(static_cast<unsigned int>(result3), 3u, "Raw XOR should equal 3");

    auto combined = FilePermission::Read | FilePermission::Write;
    auto result4 = combined ^ FilePermission::Read;
    ASSERT_EQ(static_cast<unsigned int>(result4), 2u, "(R|W) ^ R should equal 2 (Write)");
    return true;
}

bool test_bitwise_not_operators()
{
    auto perm = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    auto inverted = ~perm;
    ASSERT_EQ((inverted.underlying() & 7u), 6u, "~Read & 7 should equal 6 (Write|Execute)");

    auto result = ~FilePermission::Read;
    ASSERT_EQ((static_cast<unsigned int>(result) & 7u), 6u, "Raw ~Read & 7 should equal 6");

    auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::None);
    auto inverted2 = ~perm2;
    ASSERT_EQ((inverted2.underlying() & 7u), 7u, "~None & 7 should equal 7 (all)");
    return true;
}

bool test_compound_or_assignment()
{
    auto perm1 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    perm1 |= EnumPlusWrapper<FilePermission>(FilePermission::Write);
    ASSERT_EQ(perm1.underlying(), 3u, "Read |= Write should equal 3");

    auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    perm2 |= FilePermission::Write;
    ASSERT_EQ(perm2.underlying(), 3u, "Wrapper |= raw should equal 3");

    FilePermission perm3 = FilePermission::Read;
    perm3 |= FilePermission::Write;
    perm3 |= FilePermission::Execute;
    ASSERT_EQ(static_cast<unsigned int>(perm3), 7u, "Raw |= chain should equal 7");
    return true;
}

bool test_compound_and_assignment()
{
    auto all = FilePermission::Read | FilePermission::Write | FilePermission::Execute;
    auto perm1 = EnumPlusWrapper<FilePermission>(all);
    perm1 &= EnumPlusWrapper<FilePermission>(FilePermission::Read);
    ASSERT_EQ(perm1.underlying(), 1u, "all &= Read should equal 1");

    auto perm2 = EnumPlusWrapper<FilePermission>(all);
    perm2 &= FilePermission::Write;
    ASSERT_EQ(perm2.underlying(), 2u, "all &= Write should equal 2");

    auto perm3 = FilePermission::Read | FilePermission::Write;
    perm3 &= FilePermission::Read;
    ASSERT_EQ(static_cast<unsigned int>(perm3), 1u, "Raw &= should equal 1");
    return true;
}

bool test_compound_xor_assignment()
{
    auto perm1 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    perm1 ^= EnumPlusWrapper<FilePermission>(FilePermission::Write);
    ASSERT_EQ(perm1.underlying(), 3u, "Read ^= Write should equal 3");

    auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    perm2 ^= FilePermission::Write;
    ASSERT_EQ(perm2.underlying(), 3u, "Wrapper ^= raw should equal 3");

    auto perm3 = FilePermission::Read | FilePermission::Write;
    perm3 ^= FilePermission::Read;
    ASSERT_EQ(static_cast<unsigned int>(perm3), 2u, "(R|W) ^= R should equal 2");
    return true;
}

bool test_mixed_operations()
{
    FilePermission perm = FilePermission::None;
    perm |= FilePermission::Read;
    perm |= FilePermission::Write;
    perm &= (FilePermission::Read | FilePermission::Execute);
    ASSERT_EQ(static_cast<unsigned int>(perm), 1u, "Mixed ops should equal 1 (Read)");

    auto perm_wrap = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    perm_wrap |= FilePermission::Write;
    perm_wrap &= FilePermission::Read;
    ASSERT_EQ(perm_wrap.underlying(), 1u, "Wrapper mixed ops should equal 1");
    return true;
}

bool test_has_flag_utility()
{
    auto perms = FilePermission::Read | FilePermission::Write;

    SIMPLE_ASSERT(has_flag(perms, FilePermission::Read), "Should have Read flag");
    SIMPLE_ASSERT(has_flag(perms, FilePermission::Write), "Should have Write flag");
    SIMPLE_ASSERT(!has_flag(perms, FilePermission::Execute), "Should not have Execute flag");

    auto wrapped = EnumPlusWrapper<FilePermission>(perms);
    SIMPLE_ASSERT(has_flag(wrapped, FilePermission::Read), "Wrapped should have Read");
    SIMPLE_ASSERT(!has_flag(wrapped, FilePermission::Execute), "Wrapped should not have Execute");

    auto wrapped_flag = EnumPlusWrapper<FilePermission>(FilePermission::Read);
    SIMPLE_ASSERT(has_flag(wrapped, wrapped_flag), "Wrapper with wrapper flag should work");
    return true;
}

bool test_enum_plus_wrapper()
{
    EnumPlusWrapper<Color> color(Color::Red);
    ASSERT_EQ(color.value(), Color::Red, "Wrapper value should be Red");
    ASSERT_EQ(color.underlying(), 0, "Wrapper underlying should be 0");

    Color raw = color;
    ASSERT_EQ(raw, Color::Red, "Implicit conversion to raw enum should work");

    SIMPLE_ASSERT(color == Color::Red, "Wrapper should equal Color::Red");
    SIMPLE_ASSERT(color != Color::Blue, "Wrapper should not equal Color::Blue");

    EnumPlusWrapper<Color> color2(Color::Red);
    SIMPLE_ASSERT(color == color2, "Two wrappers with same value should be equal");
    return true;
}

bool test_wrapper_no_implicit_conversion()
{
    static_assert(!std::is_convertible_v<int, EnumPlusWrapper<Color>>,
                  "Should not allow implicit conversion from int");
    static_assert(!std::is_constructible_v<EnumPlusWrapper<Color>, int>,
                  "Should not allow construction from int");
    return true;
}

bool test_is_valid_enum()
{
    SIMPLE_ASSERT(is_valid_enum<Color>(0), "0 should be valid Color (Red)");
    SIMPLE_ASSERT(is_valid_enum<Color>(1), "1 should be valid Color (Green)");
    SIMPLE_ASSERT(is_valid_enum<Color>(2), "2 should be valid Color (Blue)");
    SIMPLE_ASSERT(is_valid_enum<Color>(3), "3 should be valid Color (Yellow)");
    SIMPLE_ASSERT(!is_valid_enum<Color>(-1), "-1 should be invalid Color");
    SIMPLE_ASSERT(!is_valid_enum<Color>(4), "4 should be invalid Color");
    SIMPLE_ASSERT(!is_valid_enum<Color>(100), "100 should be invalid Color");

    SIMPLE_ASSERT(is_valid_enum(Color::Red), "Color::Red should be valid");
    SIMPLE_ASSERT(is_valid_enum(Color::Yellow), "Color::Yellow should be valid");

    Color invalid = static_cast<Color>(100);
    SIMPLE_ASSERT(!is_valid_enum(invalid), "Cast from 100 should be invalid");
    return true;
}

bool test_safe_enum_cast()
{
    auto r1 = safe_enum_cast<Color>(0);
    SIMPLE_ASSERT(r1.has_value(), "0 should cast to valid Color");
    ASSERT_EQ(*r1, Color::Red, "0 should cast to Color::Red");

    auto r2 = safe_enum_cast<Color>(3);
    SIMPLE_ASSERT(r2.has_value(), "3 should cast to valid Color");
    ASSERT_EQ(*r2, Color::Yellow, "3 should cast to Color::Yellow");

    auto r3 = safe_enum_cast<Color>(-1);
    SIMPLE_ASSERT(!r3.has_value(), "-1 should not cast to valid Color");

    auto r4 = safe_enum_cast<Color>(4);
    SIMPLE_ASSERT(!r4.has_value(), "4 should not cast to valid Color");

    auto r5 = safe_enum_cast<Color>(100);
    SIMPLE_ASSERT(!r5.has_value(), "100 should not cast to valid Color");
    return true;
}

bool test_safe_to_underlying()
{
    auto r1 = safe_to_underlying(Color::Red);
    SIMPLE_ASSERT(r1.has_value(), "Red should have valid underlying");
    ASSERT_EQ(*r1, 0, "Red underlying should be 0");

    auto r2 = safe_to_underlying(Color::Yellow);
    SIMPLE_ASSERT(r2.has_value(), "Yellow should have valid underlying");
    ASSERT_EQ(*r2, 3, "Yellow underlying should be 3");

    Color invalid = static_cast<Color>(100);
    auto r3 = safe_to_underlying(invalid);
    SIMPLE_ASSERT(!r3.has_value(), "Invalid enum should not have valid underlying");
    return true;
}

bool test_shift_operators()
{
    FilePermission shifted = FilePermission::Read << 1;
    ASSERT_EQ(to_underlying(shifted), 2u, "Read << 1 should be 2");

    shifted = FilePermission::Read << 2;
    ASSERT_EQ(to_underlying(shifted), 4u, "Read << 2 should be 4");

    shifted = FilePermission::Execute >> 1;
    ASSERT_EQ(to_underlying(shifted), 2u, "Execute >> 1 should be 2");

    shifted = FilePermission::Execute >> 2;
    ASSERT_EQ(to_underlying(shifted), 1u, "Execute >> 2 should be 1");

    EnumPlusWrapper<FilePermission> perm(FilePermission::Read);
    auto shifted_wrap = perm << 2;
    ASSERT_EQ(shifted_wrap.underlying(), 4u, "Wrapped Read << 2 should be 4");

    perm = EnumPlusWrapper<FilePermission>(FilePermission::Execute);
    shifted_wrap = perm >> 1;
    ASSERT_EQ(shifted_wrap.underlying(), 2u, "Wrapped Execute >> 1 should be 2");

    FilePermission p = FilePermission::Read;
    p <<= 2;
    ASSERT_EQ(to_underlying(p), 4u, "Read <<= 2 should be 4");

    p = FilePermission::Execute;
    p >>= 1;
    ASSERT_EQ(to_underlying(p), 2u, "Execute >>= 1 should be 2");
    return true;
}

bool test_enum_index()
{
    auto idx0 = enum_index(Color::Red);
    SIMPLE_ASSERT(idx0.has_value(), "Red should have valid index");
    ASSERT_EQ(*idx0, 0u, "Red index should be 0");

    auto idx1 = enum_index(Color::Green);
    SIMPLE_ASSERT(idx1.has_value(), "Green should have valid index");
    ASSERT_EQ(*idx1, 1u, "Green index should be 1");

    auto idx2 = enum_index(Color::Blue);
    SIMPLE_ASSERT(idx2.has_value(), "Blue should have valid index");
    ASSERT_EQ(*idx2, 2u, "Blue index should be 2");

    auto idx3 = enum_index(Color::Yellow);
    SIMPLE_ASSERT(idx3.has_value(), "Yellow should have valid index");
    ASSERT_EQ(*idx3, 3u, "Yellow index should be 3");

    Color invalid = static_cast<Color>(100);
    auto idx_invalid = enum_index(invalid);
    SIMPLE_ASSERT(!idx_invalid.has_value(), "Invalid enum should not have index");

    auto status_idx = enum_index(Status::Running);
    SIMPLE_ASSERT(status_idx.has_value(), "Running should have valid index");
    ASSERT_EQ(*status_idx, 1u, "Running index should be 1");
    return true;
}

bool test_enum_value()
{
    auto v0 = enum_value<Color>(0);
    SIMPLE_ASSERT(v0.has_value(), "Index 0 should be valid Color");
    ASSERT_EQ(*v0, Color::Red, "Index 0 should be Red");

    auto v1 = enum_value<Color>(1);
    SIMPLE_ASSERT(v1.has_value(), "Index 1 should be valid Color");
    ASSERT_EQ(*v1, Color::Green, "Index 1 should be Green");

    auto v2 = enum_value<Color>(2);
    SIMPLE_ASSERT(v2.has_value(), "Index 2 should be valid Color");
    ASSERT_EQ(*v2, Color::Blue, "Index 2 should be Blue");

    auto v3 = enum_value<Color>(3);
    SIMPLE_ASSERT(v3.has_value(), "Index 3 should be valid Color");
    ASSERT_EQ(*v3, Color::Yellow, "Index 3 should be Yellow");

    auto v4 = enum_value<Color>(4);
    SIMPLE_ASSERT(!v4.has_value(), "Index 4 should be invalid Color");

    auto v100 = enum_value<Color>(100);
    SIMPLE_ASSERT(!v100.has_value(), "Index 100 should be invalid Color");

    auto status_v = enum_value<Status>(2);
    SIMPLE_ASSERT(status_v.has_value(), "Index 2 should be valid Status");
    ASSERT_EQ(*status_v, Status::Completed, "Index 2 should be Completed");
    return true;
}

bool test_enum_index_value_roundtrip()
{
    for (std::size_t i = 0; i < EnumSizeTrait<Color>::size; ++i) {
        auto val = enum_value<Color>(i);
        SIMPLE_ASSERT(val.has_value(), "Index should produce valid value");
        auto idx = enum_index(*val);
        SIMPLE_ASSERT(idx.has_value(), "Value should produce valid index");
        ASSERT_EQ(*idx, i, "Roundtrip should preserve index");
    }

    auto values = enum_values<Color>();
    for (auto v : values) {
        auto idx = enum_index(v);
        SIMPLE_ASSERT(idx.has_value(), "Value should have valid index");
        auto val = enum_value<Color>(*idx);
        SIMPLE_ASSERT(val.has_value(), "Index should produce valid value");
        ASSERT_EQ(*val, v, "Roundtrip should preserve value");
    }
    return true;
}

bool test_enum_contains()
{
    SIMPLE_ASSERT(enum_contains(Color::Red), "Red should be contained");
    SIMPLE_ASSERT(enum_contains(Color::Green), "Green should be contained");
    SIMPLE_ASSERT(enum_contains(Color::Blue), "Blue should be contained");
    SIMPLE_ASSERT(enum_contains(Color::Yellow), "Yellow should be contained");

    Color invalid = static_cast<Color>(100);
    SIMPLE_ASSERT(!enum_contains(invalid), "Invalid color should not be contained");

    Color negative = static_cast<Color>(-1);
    SIMPLE_ASSERT(!enum_contains(negative), "Negative should not be contained");

    SIMPLE_ASSERT(enum_contains(Status::Idle), "Idle should be contained");
    SIMPLE_ASSERT(enum_contains(Status::Running), "Running should be contained");
    return true;
}

bool test_enum_count()
{
    static_assert(enum_count<Color> == 4, "Color count should be 4");
    static_assert(enum_count<Status> == 4, "Status count should be 4");
    static_assert(enum_count<LargeEnum> == 10, "LargeEnum count should be 10");
    static_assert(enum_count<SignedEnum> == 3, "SignedEnum count should be 3");
    static_assert(enum_count<SingleValue> == 1, "SingleValue count should be 1");

    ASSERT_EQ(enum_count<Color>, 4u, "Runtime Color count should be 4");
    ASSERT_EQ(enum_count<FilePermission>, 4u, "Runtime FilePermission count should be 4");
    return true;
}

bool test_for_each_enum()
{
    std::size_t count = 0;
    int sum = 0;
    for_each_enum<Color>([&](Color c) {
        ++count;
        sum += static_cast<int>(c);
    });
    ASSERT_EQ(count, 4u, "for_each_enum should iterate 4 times");
    ASSERT_EQ(sum, 6, "Sum of Color indices should be 0+1+2+3=6");

    std::vector<Status> collected;
    for_each_enum<Status>([&](Status s) {
        collected.push_back(s);
    });
    ASSERT_EQ(collected.size(), 4u, "Should collect 4 Status values");
    ASSERT_EQ(collected[0], Status::Idle, "First should be Idle");
    ASSERT_EQ(collected[3], Status::Failed, "Last should be Failed");
    return true;
}

bool test_enum_entries()
{
    auto entries = enum_entries<Color>();
    ASSERT_EQ(entries.size(), 4u, "Should have 4 entries");

    ASSERT_EQ(entries[0].name, std::string_view("Red"), "Entry 0 name should be Red");
    ASSERT_EQ(entries[0].value, Color::Red, "Entry 0 value should be Red");

    ASSERT_EQ(entries[1].name, std::string_view("Green"), "Entry 1 name should be Green");
    ASSERT_EQ(entries[1].value, Color::Green, "Entry 1 value should be Green");

    ASSERT_EQ(entries[2].name, std::string_view("Blue"), "Entry 2 name should be Blue");
    ASSERT_EQ(entries[2].value, Color::Blue, "Entry 2 value should be Blue");

    ASSERT_EQ(entries[3].name, std::string_view("Yellow"), "Entry 3 name should be Yellow");
    ASSERT_EQ(entries[3].value, Color::Yellow, "Entry 3 value should be Yellow");

    auto status_entries = enum_entries<Status>();
    ASSERT_EQ(status_entries.size(), 4u, "Should have 4 Status entries");
    ASSERT_EQ(status_entries[1].name, std::string_view("Running"), "Status entry 1 name");
    ASSERT_EQ(status_entries[1].value, Status::Running, "Status entry 1 value");
    return true;
}

bool test_from_string_icase()
{
    auto r1 = from_string_icase<Color>("Red");
    SIMPLE_ASSERT(r1.has_value(), "Exact case 'Red' should be valid");
    ASSERT_EQ(*r1, Color::Red, "'Red' should convert to Color::Red");

    auto r2 = from_string_icase<Color>("RED");
    SIMPLE_ASSERT(r2.has_value(), "Uppercase 'RED' should be valid");
    ASSERT_EQ(*r2, Color::Red, "'RED' should convert to Color::Red");

    auto r3 = from_string_icase<Color>("red");
    SIMPLE_ASSERT(r3.has_value(), "Lowercase 'red' should be valid");
    ASSERT_EQ(*r3, Color::Red, "'red' should convert to Color::Red");

    auto r4 = from_string_icase<Color>("rEd");
    SIMPLE_ASSERT(r4.has_value(), "Mixed case 'rEd' should be valid");
    ASSERT_EQ(*r4, Color::Red, "'rEd' should convert to Color::Red");

    auto r5 = from_string_icase<Color>("YELLOW");
    SIMPLE_ASSERT(r5.has_value(), "Uppercase 'YELLOW' should be valid");
    ASSERT_EQ(*r5, Color::Yellow, "'YELLOW' should convert to Color::Yellow");

    auto invalid1 = from_string_icase<Color>("Redd");
    SIMPLE_ASSERT(!invalid1.has_value(), "'Redd' should be invalid");

    auto invalid2 = from_string_icase<Color>("Purple");
    SIMPLE_ASSERT(!invalid2.has_value(), "'Purple' should be invalid");

    auto invalid3 = from_string_icase<Color>("");
    SIMPLE_ASSERT(!invalid3.has_value(), "Empty string should be invalid");

    Color c1 = from_string_icase_or<Color>("BLUE", Color::Red);
    ASSERT_EQ(c1, Color::Blue, "from_string_icase_or 'BLUE' should return Blue");

    Color c2 = from_string_icase_or<Color>("invalid", Color::Red);
    ASSERT_EQ(c2, Color::Red, "from_string_icase_or invalid should return default");

    auto s1 = from_string_icase<Status>("RUNNING");
    SIMPLE_ASSERT(s1.has_value(), "'RUNNING' should be valid Status");
    ASSERT_EQ(*s1, Status::Running, "'RUNNING' should convert to Status::Running");
    return true;
}

bool test_to_underlying()
{
    ASSERT_EQ(to_underlying(Color::Red), 0, "Red underlying should be 0");
    ASSERT_EQ(to_underlying(Color::Green), 1, "Green underlying should be 1");
    ASSERT_EQ(to_underlying(Color::Blue), 2, "Blue underlying should be 2");
    ASSERT_EQ(to_underlying(Color::Yellow), 3, "Yellow underlying should be 3");

    ASSERT_EQ(to_underlying(FilePermission::None), 0u, "None underlying should be 0");
    ASSERT_EQ(to_underlying(FilePermission::Read), 1u, "Read underlying should be 1");
    ASSERT_EQ(to_underlying(FilePermission::Write), 2u, "Write underlying should be 2");
    ASSERT_EQ(to_underlying(FilePermission::Execute), 4u, "Execute underlying should be 4");
    return true;
}

bool test_enum_values()
{
    auto colors = enum_values<Color>();
    ASSERT_EQ(colors.size(), 4u, "Should have 4 color values");
    ASSERT_EQ(colors[0], Color::Red, "First color should be Red");
    ASSERT_EQ(colors[1], Color::Green, "Second color should be Green");
    ASSERT_EQ(colors[2], Color::Blue, "Third color should be Blue");
    ASSERT_EQ(colors[3], Color::Yellow, "Fourth color should be Yellow");
    return true;
}

bool test_constexpr_operations()
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

bool test_no_bounds_check_policy()
{
    EnumPlusMap<Color, int, NoBoundsCheckPolicy> map{10, 20, 30, 40};
    ASSERT_EQ(map[Color::Red], 10, "NoBoundsCheck map Red should be 10");
    ASSERT_EQ(map[Color::Blue], 30, "NoBoundsCheck map Blue should be 30");
    return true;
}

bool test_bounds_check_throws()
{
    EnumPlusMap<Color, int> map{10, 20, 30, 40};
    ASSERT_EQ(map.at(Color::Red), 10, "Bounds checked at(Red) should be 10");
    ASSERT_EQ(map.at(Color::Yellow), 40, "Bounds checked at(Yellow) should be 40");
    return true;
}

} // anonymous namespace

// ============================================================================
// Main Test Entry Point
// ============================================================================

namespace fat_p::testing
{

bool test_EnumPlus()
{
    PRINT_HEADER(ENUM PLUS)

    TestRunner runner;
    get_test_config().verbose = true;

    std::cout << "\n" << colors::cyan() << "Test Suite 1: EnumSizeTrait"
              << colors::reset() << "\n";
    runner.run_test("enum_size_trait", test_enum_size_trait);

    std::cout << "\n" << colors::cyan() << "Test Suite 2: EnumPlusMap Basic Operations"
              << colors::reset() << "\n";
    runner.run_test("enum_plus_map_basic", test_enum_plus_map_basic);
    runner.run_test("enum_plus_map_access", test_enum_plus_map_access);
    runner.run_test("enum_plus_map_constructor_variants", test_enum_plus_map_constructor_variants);
    runner.run_test("enum_plus_map_with_string_policy", test_enum_plus_map_with_string_policy);

    std::cout << "\n" << colors::cyan() << "Test Suite 3: Stream Operators"
              << colors::reset() << "\n";
    runner.run_test("stream_operators", test_stream_operators);

    std::cout << "\n" << colors::cyan() << "Test Suite 4: Bitwise OR Operators"
              << colors::reset() << "\n";
    runner.run_test("bitwise_or_operators", test_bitwise_or_operators);

    std::cout << "\n" << colors::cyan() << "Test Suite 5: Bitwise AND Operators"
              << colors::reset() << "\n";
    runner.run_test("bitwise_and_operators", test_bitwise_and_operators);

    std::cout << "\n" << colors::cyan() << "Test Suite 6: Bitwise XOR Operators"
              << colors::reset() << "\n";
    runner.run_test("bitwise_xor_operators", test_bitwise_xor_operators);

    std::cout << "\n" << colors::cyan() << "Test Suite 7: Bitwise NOT Operators"
              << colors::reset() << "\n";
    runner.run_test("bitwise_not_operators", test_bitwise_not_operators);

    std::cout << "\n" << colors::cyan() << "Test Suite 8: Compound Assignment Operators"
              << colors::reset() << "\n";
    runner.run_test("compound_or_assignment", test_compound_or_assignment);
    runner.run_test("compound_and_assignment", test_compound_and_assignment);
    runner.run_test("compound_xor_assignment", test_compound_xor_assignment);

    std::cout << "\n" << colors::cyan() << "Test Suite 9: Mixed Operations and Utilities"
              << colors::reset() << "\n";
    runner.run_test("mixed_operations", test_mixed_operations);
    runner.run_test("has_flag_utility", test_has_flag_utility);
    runner.run_test("enum_plus_wrapper", test_enum_plus_wrapper);
    runner.run_test("to_underlying", test_to_underlying);
    runner.run_test("enum_values", test_enum_values);
    runner.run_test("constexpr_operations", test_constexpr_operations);
    runner.run_test("no_bounds_check_policy", test_no_bounds_check_policy);
    runner.run_test("bounds_check_throws", test_bounds_check_throws);

    std::cout << "\n" << colors::cyan() << "Test Suite 10: Type Safety"
              << colors::reset() << "\n";
    runner.run_test("wrapper_no_implicit_conversion", test_wrapper_no_implicit_conversion);
    runner.run_test("is_valid_enum", test_is_valid_enum);
    runner.run_test("safe_enum_cast", test_safe_enum_cast);
    runner.run_test("safe_to_underlying", test_safe_to_underlying);
    runner.run_test("shift_operators", test_shift_operators);
    runner.run_test("from_string_icase", test_from_string_icase);

    std::cout << "\n" << colors::cyan() << "Test Suite 11: Index/Value Reflection"
              << colors::reset() << "\n";
    runner.run_test("enum_index", test_enum_index);
    runner.run_test("enum_value", test_enum_value);
    runner.run_test("enum_index_value_roundtrip", test_enum_index_value_roundtrip);

    std::cout << "\n" << colors::cyan() << "Test Suite 12: Enum Reflection Utilities"
              << colors::reset() << "\n";
    runner.run_test("enum_contains", test_enum_contains);
    runner.run_test("enum_count", test_enum_count);
    runner.run_test("for_each_enum", test_for_each_enum);
    runner.run_test("enum_entries", test_enum_entries);

    int failed = runner.print_summary();
    return failed == 0;
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_EnumPlus() ? 0 : 1;
}
#endif
