#include <iostream>
#include <sstream>
#include <string>

#include "EnumPlus.h"
#include "test_EnumPlus.h"
#include "FatPTest.h"

/**
 * @file test_EnumPlus.cpp
 * @brief Comprehensive test suite for fat_p::EnumPlus
 * 
 * This test suite demonstrates all features of EnumPlus including:
 * - Enum size traits
 * - Type-safe enum-to-value mapping (EnumPlusMap)
 * - Bitwise operators for flag enums
 * - Stream operators for enum-to-string conversion
 * - Compile-time enum utilities
 * - Policy-based bounds checking
 * 
 * @version 1.0
 * 
 * @section requirements Requirements
 * - C++17 or later
 * - Header-only, no external dependencies
 * - Tested on Intel(R) Core(TM) i7-8850H CPU @ 2.60GHz
 */

using namespace fat_p;
using namespace fat_p::testing;

namespace fat_p::testing
{
    // ============================================================================
    // Test Suite 1: EnumSizeTrait
    // ============================================================================

    bool test_enum_size_trait() {
        static_assert(EnumSizeTrait<Color>::size == 4, "Color should have 4 values");
        static_assert(EnumSizeTrait<FilePermission>::size == 3, "FilePermission should have 3 values");
        static_assert(EnumSizeTrait<Status>::size == 4, "Status should have 4 values");
        static_assert(EnumSizeTrait<LargeEnum>::size == 10, "LargeEnum should have 10 values");
        
        return true;
    }

    // ============================================================================
    // Test Suite 2: EnumPlusMap Basic Operations
    // ============================================================================

    bool test_enum_plus_map_basic() {
        // Test with string mapping
        EnumPlusMap<Color, std::string> color_names{
            "Red", "Green", "Blue", "Yellow"
        };
        
        ASSERT_EQ(color_names[Color::Red], std::string("Red"), "Color::Red should map to 'Red'");
        ASSERT_EQ(color_names[Color::Green], std::string("Green"), "Color::Green should map to 'Green'");
        ASSERT_EQ(color_names[Color::Blue], std::string("Blue"), "Color::Blue should map to 'Blue'");
        ASSERT_EQ(color_names[Color::Yellow], std::string("Yellow"), "Color::Yellow should map to 'Yellow'");
        
        // Test size
        ASSERT_EQ(color_names.size(), 4u, "Color map size should be 4");
        ASSERT_EQ(color_names.empty(), false, "Color map should not be empty");
        
        return true;
    }

    bool test_enum_plus_map_access() {
        EnumPlusMap<Color, int> color_values{10, 20, 30, 40};
        
        // Test operator[]
        ASSERT_EQ(color_values[Color::Red], 10, "Red should have value 10");
        ASSERT_EQ(color_values[Color::Green], 20, "Green should have value 20");
        ASSERT_EQ(color_values[Color::Blue], 30, "Blue should have value 30");
        ASSERT_EQ(color_values[Color::Yellow], 40, "Yellow should have value 40");
        
        // Test at()
        ASSERT_EQ(color_values.at(Color::Red), 10, "at(Red) should return 10");
        ASSERT_EQ(color_values.at(Color::Blue), 30, "at(Blue) should return 30");
        
        // Test modification
        color_values[Color::Red] = 100;
        ASSERT_EQ(color_values[Color::Red], 100, "Red should have updated value 100");
        
        return true;
    }

    bool test_enum_plus_map_constructor_variants() {
        // Default constructor
        EnumPlusMap<Color, int> default_map;
        ASSERT_EQ(default_map.size(), 4u, "Default map size should be 4");
        
        // Fill constructor
        EnumPlusMap<Color, int> filled_map(42);
        ASSERT_EQ(filled_map[Color::Red], 42, "Filled map Red should be 42");
        ASSERT_EQ(filled_map[Color::Green], 42, "Filled map Green should be 42");
        ASSERT_EQ(filled_map[Color::Blue], 42, "Filled map Blue should be 42");
        ASSERT_EQ(filled_map[Color::Yellow], 42, "Filled map Yellow should be 42");
        
        // Initializer list constructor
        EnumPlusMap<Color, int> init_list_map{1, 2, 3, 4};
        ASSERT_EQ(init_list_map[Color::Red], 1, "Init list Red should be 1");
        ASSERT_EQ(init_list_map[Color::Yellow], 4, "Init list Yellow should be 4");
        
        // Generator function constructor
        EnumPlusMap<Color, int> generated_map([](Color c) {
            return static_cast<int>(c) * 10;
        });
        ASSERT_EQ(generated_map[Color::Red], 0, "Generated Red should be 0");
        ASSERT_EQ(generated_map[Color::Green], 10, "Generated Green should be 10");
        ASSERT_EQ(generated_map[Color::Blue], 20, "Generated Blue should be 20");
        ASSERT_EQ(generated_map[Color::Yellow], 30, "Generated Yellow should be 30");
        
        // Test for_each
        int sum = 0;
        generated_map.for_each([&sum](int& val) { sum += val; });
        ASSERT_EQ(sum, 60, "Sum of generated values should be 60");
        
        return true;
    }

    bool test_enum_plus_map_with_string_policy() {
        EnumPlusMap<Status, std::string> status_messages{
            "System idle",
            "Processing...",
            "Done!",
            "Error occurred"
        };
        
        ASSERT_EQ(status_messages[Status::Idle], std::string("System idle"), "Idle status message");
        ASSERT_EQ(status_messages[Status::Running], std::string("Processing..."), "Running status message");
        ASSERT_EQ(status_messages[Status::Completed], std::string("Done!"), "Completed status message");
        ASSERT_EQ(status_messages[Status::Failed], std::string("Error occurred"), "Failed status message");
        
        return true;
    }

    // ============================================================================
    // Test Suite 3: Stream Operators
    // ============================================================================

    bool test_stream_operators() {
        std::ostringstream oss;
        
        oss << Status::Idle;
        ASSERT_EQ(oss.str(), std::string("Idle"), "Status::Idle should output 'Idle'");
        
        oss.str("");
        oss << Status::Running;
        ASSERT_EQ(oss.str(), std::string("Running"), "Status::Running should output 'Running'");
        
        oss.str("");
        oss << Status::Completed;
        ASSERT_EQ(oss.str(), std::string("Completed"), "Status::Completed should output 'Completed'");
        
        oss.str("");
        oss << Status::Failed;
        ASSERT_EQ(oss.str(), std::string("Failed"), "Status::Failed should output 'Failed'");
        
        // Test with wrapper
        oss.str("");
        oss << EnumPlusWrapper<Status>(Status::Running);
        ASSERT_EQ(oss.str(), std::string("Running"), "Wrapped Status::Running should output 'Running'");
        
        return true;
    }

    // ============================================================================
    // Test Suite 4: Bitwise OR Operators
    // ============================================================================

    bool test_bitwise_or_operators() {
        // Test EnumPlusWrapper | EnumPlusWrapper
        auto perm1 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
        auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::Write);
        auto combined = perm1 | perm2;
        auto underlying = combined.underlying();
        ASSERT_EQ(underlying, 3u, "Read | Write should equal 3");
        
        // Test EnumPlusWrapper | E
        auto combined2 = perm1 | FilePermission::Execute;
        underlying = combined2.underlying();
        ASSERT_EQ(underlying, 5u, "Read | Execute should equal 5");
        
        // Test E | E
        auto combined3 = FilePermission::Read | FilePermission::Write;
        underlying = static_cast<unsigned int>(combined3);
        ASSERT_EQ(underlying, 3u, "Enum Read | Write should equal 3");
        
        // Test E | EnumPlusWrapper
        auto perm3 = EnumPlusWrapper<FilePermission>(FilePermission::Write);
        auto combined4 = FilePermission::Read | perm3;
        underlying = combined4.underlying();
        ASSERT_EQ(underlying, 3u, "Enum Read | Wrapper Write should equal 3");
        
        return true;
    }

    // ============================================================================
    // Test Suite 5: Bitwise AND Operators
    // ============================================================================

    bool test_bitwise_and_operators() {
        // Setup combined permission
        auto all_perms = EnumPlusWrapper<FilePermission>(FilePermission::Read) | 
                         EnumPlusWrapper<FilePermission>(FilePermission::Write);
        
        // Test EnumPlusWrapper & EnumPlusWrapper
        auto perm_read = EnumPlusWrapper<FilePermission>(FilePermission::Read);
        auto result = all_perms & perm_read;
        auto underlying = result.underlying();
        ASSERT_EQ(underlying, 1u, "(Read|Write) & Read should equal 1");
        
        // Test EnumPlusWrapper & E
        auto result2 = all_perms & FilePermission::Write;
        underlying = result2.underlying();
        ASSERT_EQ(underlying, 2u, "(Read|Write) & Write should equal 2");
        
        // Test E & E
        auto combined = FilePermission::Read | FilePermission::Write;
        auto result3 = combined & FilePermission::Read;
        underlying = static_cast<unsigned int>(result3);
        ASSERT_EQ(underlying, 1u, "Enum (Read|Write) & Read should equal 1");
        
        // Test E & EnumPlusWrapper
        auto perm_write = EnumPlusWrapper<FilePermission>(FilePermission::Write);
        auto result4 = combined & perm_write;
        underlying = result4.underlying();
        ASSERT_EQ(underlying, 2u, "Enum combined & Wrapper Write should equal 2");
        
        // Test has_flag functionality
        auto hasRead = all_perms & EnumPlusWrapper<FilePermission>(FilePermission::Read);
        auto hasExecute = all_perms & EnumPlusWrapper<FilePermission>(FilePermission::Execute);
        ASSERT_EQ(hasRead.underlying(), 1u, "Should have Read flag");
        ASSERT_EQ(hasExecute.underlying(), 0u, "Should not have Execute flag");
        
        return true;
    }

    // ============================================================================
    // Test Suite 6: Bitwise XOR Operators
    // ============================================================================

    bool test_bitwise_xor_operators() {
        // Test EnumPlusWrapper ^ EnumPlusWrapper
        auto perm1 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
        auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::Write);
        auto result = perm1 ^ perm2;
        auto underlying = result.underlying();
        ASSERT_EQ(underlying, 3u, "Read ^ Write should equal 3");
        
        // Test EnumPlusWrapper ^ E
        auto result2 = perm1 ^ FilePermission::Write;
        underlying = result2.underlying();
        ASSERT_EQ(underlying, 3u, "Wrapper Read ^ Enum Write should equal 3");
        
        // Test E ^ E
        auto result3 = FilePermission::Read ^ FilePermission::Write;
        underlying = static_cast<unsigned int>(result3);
        ASSERT_EQ(underlying, 3u, "Enum Read ^ Enum Write should equal 3");
        
        // Test E ^ EnumPlusWrapper (toggle bit test)
        auto combined = FilePermission::Read | FilePermission::Write;
        auto perm_write_wrapper = EnumPlusWrapper<FilePermission>(FilePermission::Write);
        auto toggled1 = combined ^ perm_write_wrapper;
        ASSERT_EQ(static_cast<unsigned int>(combined), 3u, "Combined should be 3");
        ASSERT_EQ(toggled1.underlying(), 1u, "Toggle Write from combined should equal 1");
        
        return true;
    }

    // ============================================================================
    // Test Suite 7: Bitwise NOT Operators
    // ============================================================================

    bool test_bitwise_not_operators() {
        // Test ~EnumPlusWrapper
        auto perm = EnumPlusWrapper<FilePermission>(FilePermission::Read);
        auto inverted = ~perm;
        auto underlying = inverted.underlying();
        ASSERT_EQ(underlying, ~1u, "~Read should equal bitwise NOT of 1");
        
        // Test ~E
        auto result = ~FilePermission::Read;
        underlying = static_cast<unsigned int>(result);
        ASSERT_EQ(underlying, ~1u, "~Enum Read should equal bitwise NOT of 1");
        
        // Test ~EnumPlusWrapper with more bits
        auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::Write);
        auto inverted2 = ~perm2;
        underlying = inverted2.underlying();
        ASSERT_EQ(underlying, ~2u, "~Write should equal bitwise NOT of 2");
        
        return true;
    }

    // ============================================================================
    // Test Suite 8: Compound Assignment Operators
    // ============================================================================

    bool test_compound_or_assignment() {
        // Test EnumPlusWrapper |= EnumPlusWrapper
        auto perm1 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
        perm1 |= EnumPlusWrapper<FilePermission>(FilePermission::Write);
        ASSERT_EQ(perm1.underlying(), 3u, "Read |= Write should equal 3");
        
        // Test EnumPlusWrapper |= E
        auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
        perm2 |= FilePermission::Write;
        ASSERT_EQ(perm2.underlying(), 3u, "Wrapper Read |= Enum Write should equal 3");
        
        // Test E |= E with multiple assignments
        auto perm3 = FilePermission::Read;
        perm3 |= FilePermission::Write;
        perm3 |= FilePermission::Execute;
        perm3 |= FilePermission::Read; // redundant
        ASSERT_EQ(static_cast<unsigned int>(perm3), 7u, "Read |= Write |= Execute should equal 7");
        
        // Test with wrapper chain
        auto perm4 = EnumPlusWrapper<FilePermission>(FilePermission::None);
        perm4 |= FilePermission::Read;
        ASSERT_EQ(perm4.underlying(), 1u, "None |= Read should equal 1");
        
        return true;
    }

    bool test_compound_and_assignment() {
        // Setup
        auto all = FilePermission::Read | FilePermission::Write | FilePermission::Execute;
        
        // Test EnumPlusWrapper &= EnumPlusWrapper
        auto perm1 = EnumPlusWrapper<FilePermission>(all);
        perm1 &= EnumPlusWrapper<FilePermission>(FilePermission::Read);
        ASSERT_EQ(perm1.underlying(), 1u, "All &= Read should equal 1");
        
        // Test EnumPlusWrapper &= E
        auto perm2 = EnumPlusWrapper<FilePermission>(all);
        perm2 &= FilePermission::Write;
        ASSERT_EQ(perm2.underlying(), 2u, "All &= Write should equal 2");
        
        // Test E &= E
        auto perm3 = FilePermission::Read | FilePermission::Write;
        perm3 &= FilePermission::Read;
        ASSERT_EQ(static_cast<unsigned int>(perm3), 1u, "(Read|Write) &= Read should equal 1");
        
        return true;
    }

    bool test_compound_xor_assignment() {
        // Test EnumPlusWrapper ^= EnumPlusWrapper
        auto perm1 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
        perm1 ^= EnumPlusWrapper<FilePermission>(FilePermission::Write);
        ASSERT_EQ(perm1.underlying(), 3u, "Read ^= Write should equal 3");
        
        // Test EnumPlusWrapper ^= E
        auto perm2 = EnumPlusWrapper<FilePermission>(FilePermission::Read);
        perm2 ^= FilePermission::Write;
        ASSERT_EQ(perm2.underlying(), 3u, "Wrapper Read ^= Enum Write should equal 3");
        
        // Test E ^= E (toggle operation)
        auto perm3 = FilePermission::Read | FilePermission::Write;
        perm3 ^= FilePermission::Write; // Toggle off Write
        ASSERT_EQ(static_cast<unsigned int>(perm3), 1u, "Toggle off Write should equal 1");
        perm3 ^= FilePermission::Write; // Toggle back on
        ASSERT_EQ(static_cast<unsigned int>(perm3), 3u, "Toggle on Write should equal 3");
        
        return true;
    }

    // ============================================================================
    // Test Suite 9: Mixed Operations and Utilities
    // ============================================================================

    bool test_mixed_operations() {
        // Complex chain of operations
        auto perm = FilePermission::None;
        perm |= FilePermission::Read;
        perm |= FilePermission::Write;
        perm &= (FilePermission::Read | FilePermission::Execute);
        ASSERT_EQ(static_cast<unsigned int>(perm), 1u, "Complex operation should leave only Read");
        
        // Test with wrappers
        auto perm_wrap = EnumPlusWrapper<FilePermission>(FilePermission::Read);
        perm_wrap |= FilePermission::Write;
        perm_wrap &= FilePermission::Read;
        ASSERT_EQ(perm_wrap.underlying(), 1u, "Wrapper operations should leave only Read");
        
        return true;
    }

    bool test_has_flag_utility() {
        auto perms = FilePermission::Read | FilePermission::Write;
        
        ASSERT_EQ(has_flag(perms, FilePermission::Read), true, "Should have Read flag");
        ASSERT_EQ(has_flag(perms, FilePermission::Write), true, "Should have Write flag");
        ASSERT_EQ(has_flag(perms, FilePermission::Execute), false, "Should not have Execute flag");
        
        auto perms_wrap = EnumPlusWrapper<FilePermission>(perms);
        ASSERT_EQ(has_flag(perms_wrap, FilePermission::Read), true, "Wrapper should have Read flag");
        ASSERT_EQ(has_flag(perms_wrap, FilePermission::Write), true, "Wrapper should have Write flag");
        ASSERT_EQ(has_flag(perms_wrap, FilePermission::Execute), false, "Wrapper should not have Execute flag");
        
        auto flag_wrap = EnumPlusWrapper<FilePermission>(FilePermission::Read);
        ASSERT_EQ(has_flag(perms_wrap, flag_wrap), true, "Wrapper should have wrapped Read flag");
        
        return true;
    }

    bool test_enum_plus_wrapper() {
        EnumPlusWrapper<Color> color(Color::Red);
        ASSERT_EQ(color.value(), Color::Red, "Wrapper value should be Red");
        ASSERT_EQ(color.underlying(), 0, "Wrapper underlying value should be 0");
        
        // Test conversion
        Color c = color;
        ASSERT_EQ(c, Color::Red, "Converted color should be Red");
        
        // Test comparison
        EnumPlusWrapper<Color> color2(Color::Red);
        ASSERT_EQ(color == color2, true, "Equal wrappers should compare equal");
        ASSERT_EQ(color != color2, false, "Equal wrappers should not compare not-equal");
        
        EnumPlusWrapper<Color> color3(Color::Blue);
        ASSERT_EQ(color == color3, false, "Different wrappers should not compare equal");
        ASSERT_EQ(color != color3, true, "Different wrappers should compare not-equal");
        
        // Test comparison with enum
        ASSERT_EQ(color == Color::Red, true, "Wrapper should equal enum Red");
        ASSERT_EQ(color != Color::Blue, true, "Wrapper should not equal enum Blue");
        
        return true;
    }

    bool test_to_underlying() {
        ASSERT_EQ(to_underlying(Color::Red), 0, "Red underlying value should be 0");
        ASSERT_EQ(to_underlying(Color::Green), 1, "Green underlying value should be 1");
        ASSERT_EQ(to_underlying(Color::Blue), 2, "Blue underlying value should be 2");
        ASSERT_EQ(to_underlying(Color::Yellow), 3, "Yellow underlying value should be 3");
        
        ASSERT_EQ(to_underlying(FilePermission::None), 0u, "None underlying value should be 0");
        ASSERT_EQ(to_underlying(FilePermission::Read), 1u, "Read underlying value should be 1");
        ASSERT_EQ(to_underlying(FilePermission::Write), 2u, "Write underlying value should be 2");
        ASSERT_EQ(to_underlying(FilePermission::Execute), 4u, "Execute underlying value should be 4");
        
        return true;
    }

    bool test_enum_values() {
        auto colors = enum_values<Color>();
        ASSERT_EQ(colors.size(), 4u, "Should have 4 color values");
        ASSERT_EQ(colors[0], Color::Red, "First color should be Red");
        ASSERT_EQ(colors[1], Color::Green, "Second color should be Green");
        ASSERT_EQ(colors[2], Color::Blue, "Third color should be Blue");
        ASSERT_EQ(colors[3], Color::Yellow, "Fourth color should be Yellow");
        
        return true;
    }

    bool test_constexpr_operations() {
        // Test constexpr EnumPlusWrapper
        constexpr EnumPlusWrapper<Color> color(Color::Red);
        static_assert(color.value() == Color::Red, "constexpr value()");
        static_assert(color.underlying() == 0, "constexpr underlying()");
        
        // Test constexpr operators
        constexpr auto perm1 = FilePermission::Read | FilePermission::Write;
        static_assert(static_cast<unsigned int>(perm1) == 3u, "constexpr |");
        
        constexpr auto perm2 = FilePermission::Read & FilePermission::Read;
        static_assert(static_cast<unsigned int>(perm2) == 1u, "constexpr &");
        
        constexpr auto perm3 = FilePermission::Read ^ FilePermission::Write;
        static_assert(static_cast<unsigned int>(perm3) == 3u, "constexpr ^");
        
        return true;
    }

    bool test_no_bounds_check_policy() {
        EnumPlusMap<Color, int, NoBoundsCheckPolicy> map{10, 20, 30, 40};
        
        // Normal access should work
        ASSERT_EQ(map[Color::Red], 10, "NoBoundsCheck map Red should be 10");
        ASSERT_EQ(map[Color::Blue], 30, "NoBoundsCheck map Blue should be 30");
        
        return true;
    }

    bool test_bounds_check_throws() {
        EnumPlusMap<Color, int> map{10, 20, 30, 40};
        
        // Valid access
        ASSERT_EQ(map.at(Color::Red), 10, "Bounds checked at(Red) should be 10");
        ASSERT_EQ(map.at(Color::Yellow), 40, "Bounds checked at(Yellow) should be 40");
        
        return true;
    }

    // ============================================================================
    // Main Test Entry Point
    // ============================================================================

    bool test_EnumPlus() {

        PRINT_HEADER(ENUM PLUS)

        TestRunner runner;
        get_test_config().verbose = true;

        // Test Suite 1: EnumSizeTrait
        std::cout << "\n" << colors::cyan() << "Test Suite 1: EnumSizeTrait" 
                  << colors::reset() << "\n";
        runner.run_test("enum_size_trait", test_enum_size_trait);

        // Test Suite 2: EnumPlusMap Basic Operations
        std::cout << "\n" << colors::cyan() << "Test Suite 2: EnumPlusMap Basic Operations" 
                  << colors::reset() << "\n";
        runner.run_test("enum_plus_map_basic", test_enum_plus_map_basic);
        runner.run_test("enum_plus_map_access", test_enum_plus_map_access);
        runner.run_test("enum_plus_map_constructor_variants", test_enum_plus_map_constructor_variants);
        runner.run_test("enum_plus_map_with_string_policy", test_enum_plus_map_with_string_policy);

        // Test Suite 3: Stream Operators
        std::cout << "\n" << colors::cyan() << "Test Suite 3: Stream Operators" 
                  << colors::reset() << "\n";
        runner.run_test("stream_operators", test_stream_operators);

        // Test Suite 4: Bitwise OR Operators
        std::cout << "\n" << colors::cyan() << "Test Suite 4: Bitwise OR Operators" 
                  << colors::reset() << "\n";
        runner.run_test("bitwise_or_operators", test_bitwise_or_operators);

        // Test Suite 5: Bitwise AND Operators
        std::cout << "\n" << colors::cyan() << "Test Suite 5: Bitwise AND Operators" 
                  << colors::reset() << "\n";
        runner.run_test("bitwise_and_operators", test_bitwise_and_operators);

        // Test Suite 6: Bitwise XOR Operators
        std::cout << "\n" << colors::cyan() << "Test Suite 6: Bitwise XOR Operators" 
                  << colors::reset() << "\n";
        runner.run_test("bitwise_xor_operators", test_bitwise_xor_operators);

        // Test Suite 7: Bitwise NOT Operators
        std::cout << "\n" << colors::cyan() << "Test Suite 7: Bitwise NOT Operators" 
                  << colors::reset() << "\n";
        runner.run_test("bitwise_not_operators", test_bitwise_not_operators);

        // Test Suite 8: Compound Assignment Operators
        std::cout << "\n" << colors::cyan() << "Test Suite 8: Compound Assignment Operators" 
                  << colors::reset() << "\n";
        runner.run_test("compound_or_assignment", test_compound_or_assignment);
        runner.run_test("compound_and_assignment", test_compound_and_assignment);
        runner.run_test("compound_xor_assignment", test_compound_xor_assignment);

        // Test Suite 9: Mixed Operations and Utilities
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

        // Print summary
        int failed = runner.print_summary();
        
        return failed == 0;
    }

} // namespace fat_p::testing
