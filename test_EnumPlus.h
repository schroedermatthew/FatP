#ifndef FATP_TEST_ENUM_PLUS_H
#define FATP_TEST_ENUM_PLUS_H

#include "FatPTest.h"
#include "EnumPlus.h"

namespace fat_p {
namespace testing {

// Test enum types
enum class Color {
    Red = 0,
    Green = 1,
    Blue = 2,
    Yellow = 3
};

enum class FilePermission : unsigned int {
    None = 0,
    Read = 1,
    Write = 2,
    Execute = 4
};

enum class Status {
    Idle = 0,
    Running = 1,
    Completed = 2,
    Failed = 3
};

enum class LargeEnum {
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

// Main test entry point
bool test_EnumPlus();

} // namespace testing
} // namespace fat_p

// ============================================================================
// Template Specializations - MUST be in cpp_utilities namespace
// ============================================================================

namespace fat_p {

// EnumSizeTrait specializations
template<>
struct EnumSizeTrait<testing::Color> {
    static constexpr std::size_t size = 4;
};

template<>
struct EnumSizeTrait<testing::FilePermission> {
    static constexpr std::size_t size = 3;
};

template<>
struct EnumSizeTrait<testing::Status> {
    static constexpr std::size_t size = 4;
};

template<>
struct EnumSizeTrait<testing::LargeEnum> {
    static constexpr std::size_t size = 10;
};

// Enable overloaded operators for FilePermission
template<>
struct EnableOverloadedOperators<testing::FilePermission, true> {
    static constexpr bool value = true;
};

template<>
struct EnableOverloadedOperators<testing::FilePermission> {
    static constexpr bool value = true;
};

// EnumStringPolicy for Color
template<>
struct EnumStringPolicy<testing::Color> {
    static constexpr bool has_names = true;
    
    static std::string_view to_string(testing::Color value) {
        switch (value) {
            case testing::Color::Red: return "Red";
            case testing::Color::Green: return "Green";
            case testing::Color::Blue: return "Blue";
            case testing::Color::Yellow: return "Yellow";
            default: return "Unknown";
        }
    }
    
    static testing::Color from_string(std::string_view str) {
        if (str == "Red") return testing::Color::Red;
        if (str == "Green") return testing::Color::Green;
        if (str == "Blue") return testing::Color::Blue;
        if (str == "Yellow") return testing::Color::Yellow;
        throw std::invalid_argument("Invalid Color string");
    }
};

// EnumStringPolicy for Status
template<>
struct EnumStringPolicy<testing::Status> {
    static constexpr bool has_names = true;
    
    static std::string_view to_string(testing::Status value) {
        switch (value) {
            case testing::Status::Idle: return "Idle";
            case testing::Status::Running: return "Running";
            case testing::Status::Completed: return "Completed";
            case testing::Status::Failed: return "Failed";
            default: return "Unknown";
        }
    }
    
    static testing::Status from_string(std::string_view str) {
        if (str == "Idle") return testing::Status::Idle;
        if (str == "Running") return testing::Status::Running;
        if (str == "Completed") return testing::Status::Completed;
        if (str == "Failed") return testing::Status::Failed;
        throw std::invalid_argument("Invalid Status string");
    }
};

} // namespace fat_p

#endif // FATP_TEST_ENUM_PLUS_H
