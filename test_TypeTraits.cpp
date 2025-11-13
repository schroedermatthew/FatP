#include <vector>
#include <list>
#include <deque>
#include <array>
#include <string>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <forward_list>
#include <iostream>
#include <sstream>
#include <optional>
#include <variant>
#include <tuple>

#include "TypeTraits.h"
#include "test_TypeTraits.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

enum class ScopedEnum { A, B, C };
enum UnscopedEnum { X, Y, Z };

struct TransparentComparator {
    using is_transparent = void;
    template<typename T, typename U>
    bool operator()(const T& a, const U& b) const {
        return a < b;
    }
};

struct NonTransparentComparator {
    bool operator()(int a, int b) const { return a < b; }
};

struct ContainerWithClear {
    using value_type = int;
    int* begin() { return nullptr; }
    int* end() { return nullptr; }
    size_t size() const { return 0; }
    void clear() {}
};

struct ContainerWithPushBack {
    using value_type = int;
    int* begin() { return nullptr; }
    int* end() { return nullptr; }
    size_t size() const { return 0; }
    void push_back(const int&) {}
};

struct ContainerWithEmplaceBack {
    using value_type = int;
    int* begin() { return nullptr; }
    int* end() { return nullptr; }
    size_t size() const { return 0; }
    template<typename... Args>
    void emplace_back(Args&&...) {}
};

struct ContainerWithPushFront {
    using value_type = int;
    int* begin() { return nullptr; }
    int* end() { return nullptr; }
    size_t size() const { return 0; }
    void push_front(const int&) {}
};

struct TriviallyRelocatable {
    int value;
};

struct NonTriviallyRelocatable {
    NonTriviallyRelocatable() = default;
    NonTriviallyRelocatable(const NonTriviallyRelocatable&) {}
    NonTriviallyRelocatable& operator=(const NonTriviallyRelocatable&) { return *this; }
    int value;
};

struct WithCustomMember {
    int custom_value;
};

struct WithCustomMethod {
    void custom_method() {}
};

DEFINE_HAS_MEMBER(custom_value)
DEFINE_HAS_METHOD(custom_method)

bool test_type_traits_scoped_enum() {
    static_assert(is_scoped_enum_v<ScopedEnum>, "ScopedEnum");
    static_assert(!is_scoped_enum_v<UnscopedEnum>, "UnscopedEnum");
    static_assert(!is_scoped_enum_v<int>, "int");
    
    return true;
}

bool test_type_traits_transparent() {
    static_assert(is_transparent_v<TransparentComparator>, "TransparentComparator");
    static_assert(!is_transparent_v<NonTransparentComparator>, "NonTransparentComparator");
    static_assert(!is_transparent_v<int>, "int");
    
    return true;
}

bool test_type_traits_container_operations() {
    static_assert(has_clear_v<std::vector<int>>, "vector");
    static_assert(has_clear_v<ContainerWithClear>, "ContainerWithClear");
    static_assert(!has_clear_v<int>, "int");
    
    static_assert(has_push_back_v<std::vector<int>>, "vector");
    static_assert(has_push_back_v<ContainerWithPushBack>, "ContainerWithPushBack");
    static_assert(!has_push_back_v<std::array<int, 5>>, "array");
    
    static_assert(has_emplace_back_v<std::vector<int>>, "vector");
    static_assert(has_emplace_back_v<ContainerWithEmplaceBack>, "ContainerWithEmplaceBack");
    static_assert(!has_emplace_back_v<std::array<int, 5>>, "array");
    
    static_assert(has_push_front_v<std::deque<int>>, "deque");
    static_assert(has_push_front_v<ContainerWithPushFront>, "ContainerWithPushFront");
    static_assert(!has_push_front_v<std::vector<int>>, "vector");
    
    return true;
}

bool test_type_traits_contiguous_container() {
    static_assert(is_contiguous_container_v<std::vector<int>>, "vector");
    static_assert(is_contiguous_container_v<std::array<int, 5>>, "array");
    static_assert(is_contiguous_container_v<std::string>, "string");
    
    static_assert(!is_contiguous_container_v<std::list<int>>, "list");
    static_assert(!is_contiguous_container_v<std::deque<int>>, "deque");
    
    return true;
}

bool test_type_traits_trivially_relocatable() {
    static_assert(is_trivially_relocatable_v<int>, "int");
    static_assert(is_trivially_relocatable_v<TriviallyRelocatable>, "TriviallyRelocatable");
    static_assert(!is_trivially_relocatable_v<NonTriviallyRelocatable>, "NonTriviallyRelocatable");
    static_assert(!is_trivially_relocatable_v<std::string>, "string");
    
    return true;
}

bool test_type_traits_composition() {
    using namespace trait_ops;
    
    static_assert(all_of_v<std::vector<int>, is_iterable, is_sized, has_reserve>, "vector all");
    static_assert(!all_of_v<std::list<int>, is_iterable, is_sized, has_reserve>, "list all");
    
    static_assert(any_of_v<std::vector<int>, has_reserve, has_push_front>, "vector any");
    static_assert(!any_of_v<std::array<int, 5>, has_reserve, has_clear>, "array any");
    
    static_assert(none_of_v<int, is_iterable>, "int none");
    static_assert(!none_of_v<std::vector<int>, is_iterable>, "vector none");
    
    return true;
}

bool test_type_traits_macros() {
    static_assert(has_custom_value_v<WithCustomMember>, "WithCustomMember");
    static_assert(!has_custom_value_v<int>, "int");
    
    static_assert(has_custom_method_v<WithCustomMethod>, "WithCustomMethod");
    static_assert(!has_custom_method_v<int>, "int");
    
    return true;
}

bool test_type_traits_dbc_helpers() {
    requires_contiguous<std::vector<int>>();
    
    return true;
}

bool test_TypeTraits() {
    PRINT_HEADER(TYPE TRAITS ENHANCED FEATURES)

    TestRunner runner;

    RUN_TEST(runner, type_traits_scoped_enum);
    RUN_TEST(runner, type_traits_transparent);
    RUN_TEST(runner, type_traits_container_operations);
    RUN_TEST(runner, type_traits_contiguous_container);
    RUN_TEST(runner, type_traits_trivially_relocatable);
    RUN_TEST(runner, type_traits_composition);
    RUN_TEST(runner, type_traits_macros);
    RUN_TEST(runner, type_traits_dbc_helpers);

    int failed = runner.print_summary();
    return failed == 0;
}

} // namespace cpp_utilities::testing
