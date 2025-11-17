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
#include <memory>
#include <functional>

#include "TypeTraits.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_TypeTraits.h"
#endif

namespace fat_p::testing
{

enum class ScopedEnum
{
    A, B, C
};

enum UnscopedEnum
{
    X, Y, Z
};

struct TransparentComparator
{
    using is_transparent = void;
    template<typename T, typename U>
    bool operator()(const T& a, const U& b) const
    {
        return a < b;
    }
};

struct NonTransparentComparator
{
    bool operator()(int a, int b) const
    {
        return a < b;
    }
};

struct ContainerWithClear
{
    using value_type = int;
    int* begin()
    {
        return nullptr;
    }
    int* end()
    {
        return nullptr;
    }
    size_t size() const
    {
        return 0;
    }
    void clear()
    {
    }
};

struct ContainerWithPushBack
{
    using value_type = int;
    int* begin()
    {
        return nullptr;
    }
    int* end()
    {
        return nullptr;
    }
    size_t size() const
    {
        return 0;
    }
    void push_back(const int&)
    {
    }
};

struct ContainerWithEmplaceBack
{
    using value_type = int;
    int* begin()
    {
        return nullptr;
    }
    int* end()
    {
        return nullptr;
    }
    size_t size() const
    {
        return 0;
    }
    template<typename... Args>
    void emplace_back(Args&&...)
    {
    }
};

struct ContainerWithPushFront
{
    using value_type = int;
    int* begin()
    {
        return nullptr;
    }
    int* end()
    {
        return nullptr;
    }
    size_t size() const
    {
        return 0;
    }
    void push_front(const int&)
    {
    }
};

struct TriviallyRelocatable
{
    int value;
};

struct NonTriviallyRelocatable
{
    NonTriviallyRelocatable() = default;
    NonTriviallyRelocatable(const NonTriviallyRelocatable&)
    {
    }
    NonTriviallyRelocatable& operator=(const NonTriviallyRelocatable&)
    {
        return *this;
    }
    int value;
};

struct WithCustomMember
{
    int custom_value;
};

struct WithCustomMethod
{
    void custom_method()
    {
    }
};

struct Serializable
{
    void serialize(std::ostream& os) const
    {
        os << value;
    }
    static Serializable deserialize(std::istream& is)
    {
        Serializable s;
        is >> s.value;
        return s;
    }
    int value;
};

struct NonSerializable
{
    int value;
};

template<typename T>
struct CustomAllocator
{
    using value_type = T;
    T* allocate(std::size_t n)
    {
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }
    void deallocate(T* p, std::size_t)
    {
        ::operator delete(p);
    }
    template<typename U>
    struct rebind
    {
        using other = CustomAllocator<U>;
    };
};

struct NotAnAllocator
{
    int value;
};

struct WithValueType
{
    using value_type = int;
};

struct WithoutValueType
{
    int data;
};

// Custom trait using is_detected pattern
template <typename T>
using has_custom_value_t = decltype(&T::custom_value);

template <typename T>
inline constexpr bool has_custom_value_v = fat_p::is_detected_v<has_custom_value_t, T>;

template <typename T>
using has_custom_method_t = decltype(std::declval<T&>().custom_method());

template <typename T>
inline constexpr bool has_custom_method_v = fat_p::is_detected_v<has_custom_method_t, T>;

struct AggregateStruct
{
    int x;
    double y;
};

struct NonAggregateStruct
{
    NonAggregateStruct(int a) : x(a)
    {
    }
    int x;
};

struct WithHasValue
{
    bool has_value() const
    {
        return true;
    }
};

struct WithIndex
{
    size_t index() const
    {
        return 0;
    }
};

bool test_detection_idiom()
{
    using namespace detail;
    static_assert(is_detected_v<op_value_type, std::vector<int>>, "vector has value_type");
    static_assert(!is_detected_v<op_value_type, int>, "int has no value_type");
    using detected_type = detected_or<double, op_value_type, std::vector<int>>;
    static_assert(std::is_same_v<detected_type, int>, "detected_or finds value_type");
    using default_type = detected_or<double, op_value_type, int>;
    static_assert(std::is_same_v<default_type, double>, "detected_or uses default");
    static_assert(is_detected_exact_v<int, op_value_type, std::vector<int>>, "exact type match");
    static_assert(!is_detected_exact_v<double, op_value_type, std::vector<int>>, "type mismatch");
    static_assert(is_detected_convertible_v<long, op_value_type, std::vector<int>>, "int converts to long");
    return true;
}

bool test_container_basic_traits()
{
    static_assert(has_begin_v<std::vector<int>>, "vector has begin");
    static_assert(has_begin_v<std::array<int, 5>>, "array has begin");
    static_assert(!has_begin_v<int>, "int has no begin");
    static_assert(has_end_v<std::vector<int>>, "vector has end");
    static_assert(has_end_v<std::list<int>>, "list has end");
    static_assert(!has_end_v<int>, "int has no end");
    static_assert(has_size_v<std::vector<int>>, "vector has size");
    static_assert(has_size_v<std::string>, "string has size");
    static_assert(!has_size_v<int>, "int has no size");
    static_assert(has_empty_v<std::vector<int>>, "vector has empty");
    static_assert(has_empty_v<std::map<int, int>>, "map has empty");
    static_assert(!has_empty_v<int>, "int has no empty");
    static_assert(has_reserve_v<std::vector<int>>, "vector has reserve");
    static_assert(has_reserve_v<std::string>, "string has reserve");
    static_assert(!has_reserve_v<std::list<int>>, "list has no reserve");
    static_assert(has_data_v<std::vector<int>>, "vector has data");
    static_assert(has_data_v<std::array<int, 5>>, "array has data");
    static_assert(!has_data_v<std::list<int>>, "list has no data");
    return true;
}

bool test_reverse_iteration_traits()
{
    static_assert(has_rbegin_v<std::vector<int>>, "vector has rbegin");
    static_assert(has_rbegin_v<std::list<int>>, "list has rbegin");
    static_assert(!has_rbegin_v<std::forward_list<int>>, "forward_list has no rbegin");
    static_assert(has_rend_v<std::vector<int>>, "vector has rend");
    static_assert(has_rend_v<std::deque<int>>, "deque has rend");
    static_assert(!has_rend_v<std::forward_list<int>>, "forward_list has no rend");
    static_assert(is_reverse_iterable_v<std::vector<int>>, "vector is reverse iterable");
    static_assert(is_reverse_iterable_v<std::string>, "string is reverse iterable");
    static_assert(!is_reverse_iterable_v<std::forward_list<int>>, "forward_list not reverse iterable");
    return true;
}

bool test_composite_container_traits()
{
    static_assert(is_iterable_v<std::vector<int>>, "vector is iterable");
    static_assert(is_iterable_v<std::list<int>>, "list is iterable");
    static_assert(is_iterable_v<std::array<int, 5>>, "array is iterable");
    static_assert(!is_iterable_v<int>, "int not iterable");
    static_assert(is_sized_v<std::vector<int>>, "vector is sized");
    static_assert(is_sized_v<std::map<int, int>>, "map is sized");
    static_assert(!is_sized_v<int>, "int not sized");
    static_assert(is_container_v<std::vector<int>>, "vector is container");
    static_assert(is_container_v<std::list<int>>, "list is container");
    static_assert(!is_container_v<int>, "int not container");
    static_assert(is_reservable_v<std::vector<int>>, "vector is reservable");
    static_assert(is_reservable_v<std::string>, "string is reservable");
    static_assert(!is_reservable_v<std::list<int>>, "list not reservable");
    return true;
}

bool test_access_traits()
{
    static_assert(has_subscript_v<std::vector<int>>, "vector has subscript");
    static_assert(has_subscript_v<std::array<int, 5>>, "array has subscript");
    static_assert(has_subscript_v<std::map<int, int>>, "map has subscript");
    static_assert(!has_subscript_v<std::list<int>>, "list has no subscript");
    static_assert(has_at_v<std::vector<int>>, "vector has at");
    static_assert(has_at_v<std::map<int, int>>, "map has at");
    static_assert(has_at_v<std::array<int, 5>>, "array has at");
    static_assert(is_random_accessible_v<std::vector<int>>, "vector is random accessible");
    static_assert(is_random_accessible_v<std::string>, "string is random accessible");
    static_assert(!is_random_accessible_v<std::list<int>>, "list not random accessible");
    return true;
}

bool test_container_operations()
{
    static_assert(has_clear_v<std::vector<int>>, "vector has clear");
    static_assert(has_clear_v<ContainerWithClear>, "custom container has clear");
    static_assert(!has_clear_v<int>, "int has no clear");
    static_assert(has_push_back_v<std::vector<int>>, "vector has push_back");
    static_assert(has_push_back_v<ContainerWithPushBack>, "custom container has push_back");
    static_assert(!has_push_back_v<std::array<int, 5>>, "array has no push_back");
    static_assert(has_emplace_back_v<std::vector<int>>, "vector has emplace_back");
    static_assert(has_emplace_back_v<ContainerWithEmplaceBack>, "custom container has emplace_back");
    static_assert(!has_emplace_back_v<std::array<int, 5>>, "array has no emplace_back");
    static_assert(has_push_front_v<std::deque<int>>, "deque has push_front");
    static_assert(has_push_front_v<ContainerWithPushFront>, "custom container has push_front");
    static_assert(!has_push_front_v<std::vector<int>>, "vector has no push_front");
    return true;
}

bool test_contiguous_container()
{
    static_assert(is_contiguous_container_v<std::vector<int>>, "vector is contiguous");
    static_assert(is_contiguous_container_v<std::array<int, 5>>, "array is contiguous");
    static_assert(is_contiguous_container_v<std::string>, "string is contiguous");
    static_assert(!is_contiguous_container_v<std::list<int>>, "list not contiguous");
    static_assert(!is_contiguous_container_v<std::deque<int>>, "deque not contiguous");
    return true;
}

bool test_map_like()
{
    static_assert(is_map_like_v<std::map<int, int>>, "map is map-like");
    static_assert(is_map_like_v<std::unordered_map<int, int>>, "unordered_map is map-like");
    static_assert(!is_map_like_v<std::vector<int>>, "vector not map-like");
    static_assert(!is_map_like_v<std::set<int>>, "set not map-like");
    return true;
}

bool test_comparison_traits()
{
    static_assert(is_hashable_v<int>, "int is hashable");
    static_assert(is_hashable_v<std::string>, "string is hashable");
    static_assert(!is_hashable_v<std::vector<int>>, "vector not hashable");
    static_assert(is_equality_comparable_v<int>, "int has operator==");
    static_assert(is_equality_comparable_v<std::string>, "string has operator==");
    static_assert(is_inequality_comparable_v<int>, "int has operator!=");
    static_assert(is_inequality_comparable_v<double>, "double has operator!=");
    static_assert(has_less_v<int>, "int has operator<");
    static_assert(has_less_v<std::string>, "string has operator<");
    static_assert(has_less_equal_v<int>, "int has operator<=");
    static_assert(has_greater_v<int>, "int has operator>");
    static_assert(has_greater_equal_v<int>, "int has operator>=");
    static_assert(is_fully_ordered_v<int>, "int is fully ordered");
    static_assert(is_fully_ordered_v<std::string>, "string is fully ordered");
    static_assert(has_less_than_v<int>, "int has operator<");
    static_assert(has_less_than_v<double>, "double has operator<");
    return true;
}

bool test_comparator_traits()
{
    static_assert(is_valid_comparator_v<std::less<int>, int>, "std::less is valid for int");
    static_assert(is_valid_comparator_v<TransparentComparator, int>, "transparent comparator valid");
    static_assert(is_valid_comparator_v<NonTransparentComparator, int>, "non-transparent comparator valid");
    static_assert(is_transparent_v<TransparentComparator>, "has is_transparent tag");
    static_assert(!is_transparent_v<NonTransparentComparator>, "lacks is_transparent tag");
    static_assert(!is_transparent_v<int>, "int not transparent");
    return true;
}

bool test_library_type_detection()
{
    static_assert(is_atomic_v<std::atomic<int>>, "atomic<int> detected");
    static_assert(is_atomic_v<std::atomic<bool>>, "atomic<bool> detected");
    static_assert(!is_atomic_v<int>, "int not atomic");
    static_assert(is_scoped_enum_v<ScopedEnum>, "scoped enum detected");
    static_assert(!is_scoped_enum_v<UnscopedEnum>, "unscoped enum not detected");
    static_assert(!is_scoped_enum_v<int>, "int not enum");
    return true;
}

bool test_serialization_traits()
{
    static_assert(has_serialize_v<Serializable>, "has serialize method");
    static_assert(!has_serialize_v<NonSerializable>, "lacks serialize method");
    static_assert(!has_serialize_v<int>, "int has no serialize");
    static_assert(has_deserialize_v<Serializable>, "has deserialize method");
    static_assert(!has_deserialize_v<NonSerializable>, "lacks deserialize method");
    static_assert(is_serializable_v<Serializable>, "fully serializable");
    static_assert(!is_serializable_v<NonSerializable>, "not serializable");
    static_assert(!is_serializable_v<int>, "int not serializable");
    return true;
}

bool test_allocator_traits()
{
    static_assert(has_allocator_type_v<std::vector<int>>, "vector has allocator_type");
    static_assert(has_allocator_type_v<std::string>, "string has allocator_type");
    static_assert(!has_allocator_type_v<int>, "int has no allocator_type");
    static_assert(is_allocator_v<std::allocator<int>>, "std::allocator is allocator");
    static_assert(is_allocator_v<CustomAllocator<int>>, "custom allocator is allocator");
    static_assert(!is_allocator_v<NotAnAllocator>, "not an allocator");
    static_assert(has_rebind_v<std::allocator<int>>, "std::allocator has rebind");
    static_assert(has_rebind_v<CustomAllocator<int>>, "custom allocator has rebind");
    static_assert(!has_rebind_v<int>, "int has no rebind");
    return true;
}

bool test_callable_traits()
{
    auto lambda = [](int x)
    {
        return x * 2;
    };
    auto void_lambda = []()
    {
    };
    static_assert(is_invocable_v<decltype(lambda), int>, "lambda invocable with int");
    static_assert(is_invocable_v<decltype(void_lambda)>, "void lambda invocable");
    static_assert(!is_invocable_v<decltype(lambda)>, "lambda needs argument");
    static_assert(is_invocable_r_v<int, decltype(lambda), int>, "lambda returns int");
    static_assert(!is_invocable_r_v<double, decltype(void_lambda)>, "void lambda returns void");
    static_assert(is_function_object_v<decltype(lambda)>, "lambda is function object");
    static_assert(is_function_object_v<std::less<int>>, "std::less is function object");
    static_assert(!is_function_object_v<int>, "int not function object");
    return true;
}

bool test_aggregate_and_array_traits()
{
    static_assert(is_aggregate_v<AggregateStruct>, "struct is aggregate");
    static_assert(!is_aggregate_v<NonAggregateStruct>, "non-aggregate has constructor");
    static_assert(!is_aggregate_v<std::vector<int>>, "vector not aggregate");
    static_assert(is_bounded_array_v<int[5]>, "bounded array detected");
    static_assert(!is_bounded_array_v<int[]>, "unbounded array not bounded");
    static_assert(!is_bounded_array_v<int>, "int not array");
    static_assert(is_unbounded_array_v<int[]>, "unbounded array detected");
    static_assert(!is_unbounded_array_v<int[5]>, "bounded array not unbounded");
    return true;
}

bool test_tuple_traits()
{
    static_assert(has_tuple_size_v<std::tuple<int, double>>, "tuple has tuple_size");
    static_assert(has_tuple_size_v<std::pair<int, double>>, "pair has tuple_size");
    static_assert(has_tuple_size_v<std::array<int, 5>>, "array has tuple_size");
    static_assert(!has_tuple_size_v<int>, "int has no tuple_size");
    static_assert(has_tuple_element_v<std::tuple<int, double>>, "tuple has tuple_element");
    static_assert(has_tuple_element_v<std::pair<int, double>>, "pair has tuple_element");
    static_assert(!has_tuple_element_v<int>, "int has no tuple_element");
    static_assert(has_get_v<std::tuple<int, double>>, "tuple has get");
    static_assert(has_get_v<std::pair<int, double>>, "pair has get");
    static_assert(!has_get_v<int>, "int has no get");
    static_assert(is_tuple_like_v<std::tuple<int, double>>, "tuple is tuple-like");
    static_assert(is_tuple_like_v<std::pair<int, double>>, "pair is tuple-like");
    static_assert(is_tuple_like_v<std::array<int, 5>>, "array is tuple-like");
    static_assert(!is_tuple_like_v<std::vector<int>>, "vector not tuple-like");
    return true;
}

bool test_iterator_traits()
{
    static_assert(has_iterator_category_v<std::vector<int>::iterator>, "vector iterator has category");
    static_assert(has_iterator_category_v<std::list<int>::iterator>, "list iterator has category");
    static_assert(has_iterator_category_v<int*>, "pointer has iterator traits");
    static_assert(!has_iterator_category_v<int>, "int has no iterator category");
    return true;
}

bool test_string_traits()
{
    static_assert(has_c_str_v<std::string>, "string has c_str");
    static_assert(!has_c_str_v<std::vector<char>>, "vector<char> has no c_str");
    static_assert(!has_c_str_v<int>, "int has no c_str");
    static_assert(is_string_like_v<std::string>, "string is string-like");
    static_assert(!is_string_like_v<std::vector<char>>, "vector<char> not string-like");
    return true;
}

bool test_optional_variant_traits()
{
    static_assert(has_has_value_v<std::optional<int>>, "optional has has_value");
    static_assert(has_has_value_v<WithHasValue>, "custom type has has_value");
    static_assert(!has_has_value_v<int>, "int has no has_value");
    static_assert(has_value_method_v<std::optional<int>>, "optional has value method");
    static_assert(!has_value_method_v<int>, "int has no value method");
    static_assert(is_optional_like_v<std::optional<int>>, "optional is optional-like");
    static_assert(has_index_method_v<std::variant<int, double>>, "variant has index");
    static_assert(has_index_method_v<WithIndex>, "custom type has index");
    static_assert(!has_index_method_v<int>, "int has no index");
    static_assert(is_variant_like_v<std::variant<int, double>>, "variant is variant-like");
    return true;
}

bool test_trivially_relocatable()
{
    static_assert(is_trivially_relocatable_v<int>, "int is trivially relocatable");
    static_assert(is_trivially_relocatable_v<TriviallyRelocatable>, "trivial struct is relocatable");
    static_assert(!is_trivially_relocatable_v<NonTriviallyRelocatable>, "non-trivial not relocatable");
    static_assert(!is_trivially_relocatable_v<std::string>, "string not trivially relocatable");
    return true;
}

bool test_trait_composition()
{
    using namespace trait_ops;
    static_assert(all_of_v<std::vector<int>, is_iterable, is_sized, has_reserve>, "vector passes all traits");
    static_assert(!all_of_v<std::list<int>, is_iterable, is_sized, has_reserve>, "list fails reserve");
    static_assert(any_of_v<std::vector<int>, has_reserve, has_push_front>, "vector has reserve");
    static_assert(!any_of_v<std::array<int, 5>, has_reserve, has_clear>, "array has neither");
    static_assert(none_of_v<int, is_iterable>, "int not iterable");
    static_assert(!none_of_v<std::vector<int>, is_iterable>, "vector is iterable");
    return true;
}

bool test_is_detected_pattern()
{
    static_assert(has_custom_value_v<WithCustomMember>, "is_detected pattern detected member");
    static_assert(!has_custom_value_v<int>, "int has no custom member");
    static_assert(has_custom_method_v<WithCustomMethod>, "is_detected pattern detected method");
    static_assert(!has_custom_method_v<int>, "int has no custom method");
    return true;
}

bool test_diagnostics()
{
    using namespace diagnostics;
    const char* reason = diagnose_container<int>();
    SIMPLE_ASSERT(reason != nullptr, "diagnostic returns reason");
    static_assert(why_not_container<std::vector<int>>::reason != nullptr, "vector diagnostic exists");
    static_assert(why_not_hashable<int>::reason != nullptr, "int hashable diagnostic exists");
    static_assert(why_not_serializable<int>::reason != nullptr, "int serializable diagnostic exists");
    static_assert(why_not_comparable<int>::reason != nullptr, "int comparable diagnostic exists");
    return true;
}

bool test_dbc_helpers()
{
    requires_iterable<std::vector<int>>();
    requires_sized<std::string>();
    requires_container<std::list<int>>();
    requires_hashable<int>();
    requires_comparable<double>();
    requires_invocable<std::less<int>, int, int>();
    requires_allocator<std::allocator<int>>();
    requires_serializable<Serializable>();
    requires_contiguous<std::vector<int>>();
    return true;
}

bool test_value_type_detection()
{
    static_assert(is_detected_v<detail::op_value_type, std::vector<int>>, "vector has value_type");
    static_assert(is_detected_v<detail::op_value_type, WithValueType>, "custom has value_type");
    static_assert(!is_detected_v<detail::op_value_type, WithoutValueType>, "no value_type");
    return true;
}

bool test_constexpr_evaluation()
{
    constexpr bool is_vec_iterable = is_iterable_v<std::vector<int>>;
    constexpr bool is_int_hashable = is_hashable_v<int>;
    constexpr bool is_string_sized = is_sized_v<std::string>;
    SIMPLE_ASSERT(is_vec_iterable, "constexpr evaluation works");
    SIMPLE_ASSERT(is_int_hashable, "constexpr hashable check");
    SIMPLE_ASSERT(is_string_sized, "constexpr sized check");
    return true;
}

void benchmark_typetraits()
{
    std::cout << "\n" << colors::cyan() << "TypeTraits Benchmarks:" << colors::reset() << "\n\n";
    std::cout << "Note: Type traits are compile-time only, so runtime benchmarks measure overhead.\n";
    double lambda_invoke_time = measure_perf([]()
    {
        auto f = [](int x)
        {
            return x * 2;
        };
        int result = f(42);
        DoNotOptimize(result);
    }, 1000000, 10000);
    std::cout << "Lambda invocation overhead: " << format_time(lambda_invoke_time) << "\n";
    double container_iteration_time = measure_perf([]()
    {
        std::vector<int> v = {1, 2, 3, 4, 5};
        int sum = 0;
        for (int x : v)
        {
            sum += x;
        }
        DoNotOptimize(sum);
    }, 100000, 1000);
    std::cout << "Container iteration (verified iterable): " << format_time(container_iteration_time) << "\n";
}

bool test_TypeTraits()
{
    PRINT_HEADER(TYPE TRAITS)
    TestRunner runner;
    RUN_TEST(runner, detection_idiom);
    RUN_TEST(runner, container_basic_traits);
    RUN_TEST(runner, reverse_iteration_traits);
    RUN_TEST(runner, composite_container_traits);
    RUN_TEST(runner, access_traits);
    RUN_TEST(runner, container_operations);
    RUN_TEST(runner, contiguous_container);
    RUN_TEST(runner, map_like);
    RUN_TEST(runner, comparison_traits);
    RUN_TEST(runner, comparator_traits);
    RUN_TEST(runner, library_type_detection);
    RUN_TEST(runner, serialization_traits);
    RUN_TEST(runner, allocator_traits);
    RUN_TEST(runner, callable_traits);
    RUN_TEST(runner, aggregate_and_array_traits);
    RUN_TEST(runner, tuple_traits);
    RUN_TEST(runner, iterator_traits);
    RUN_TEST(runner, string_traits);
    RUN_TEST(runner, optional_variant_traits);
    RUN_TEST(runner, trivially_relocatable);
    RUN_TEST(runner, trait_composition);
    RUN_TEST(runner, is_detected_pattern);
    RUN_TEST(runner, diagnostics);
    RUN_TEST(runner, dbc_helpers);
    RUN_TEST(runner, value_type_detection);
    RUN_TEST(runner, constexpr_evaluation);
    benchmark_typetraits();
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TypeTraits() ? 0 : 1;
}
#endif
