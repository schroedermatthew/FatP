/**
 * @file test_Concepts.cpp
 * @brief Comprehensive unit tests for Concepts.h
 *
 * @details Tests all concepts defined in fat_p::concepts namespace.
 */

/*
FATP_META:
  meta_version: 1
  component: Concepts
  file_role: test
  path: components/Concepts/tests/test_Concepts.cpp
  layer: Testing
  namespace: fat_p
  summary: "test file for Concepts"
  api_stability: in_work
  related:
    docs_search: "Concepts"
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


#include <array>
#include <atomic>
#include <deque>
#include <forward_list>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Concepts.h"
#include "FatPTest.h"

namespace fat_p::testing::concepts
{

// =============================================================================
// Helper Types
// =============================================================================

struct NotIterable
{
    int value;
};

struct IterableOnly
{
    int* mData;
    int mSize;

    int* begin() { return mData; }
    int* end() { return mData + mSize; }
    const int* begin() const { return mData; }
    const int* end() const { return mData + mSize; }
};

struct SizedOnly
{
    std::size_t size() const { return 10; }
};

struct ContainerLike
{
    int* mData;
    int mSize;

    int* begin() { return mData; }
    int* end() { return mData + mSize; }
    const int* begin() const { return mData; }
    const int* end() const { return mData + mSize; }
    std::size_t size() const { return static_cast<std::size_t>(mSize); }
};

struct ContiguousLike : ContainerLike
{
    int* data() { return mData; }
    const int* data() const { return mData; }
};

struct Reservable
{
    void reserve(std::size_t) {}
};

struct HashableType
{
    int value;
    bool operator==(const HashableType&) const = default;
};

struct NotHashable
{
    int value;
};

struct Streamable
{
    friend std::ostream& operator<<(std::ostream& os, const Streamable&) { return os; }
};

struct NotStreamable
{
    int value;
};

struct SerializableType
{
    void serialize(std::ostream&) {}
    static SerializableType deserialize(std::istream&) { return {}; }
};

struct HasValidate
{
    void validate() {}
};

struct HasSharedGuard
{
    struct SharedGuard
    {
    };
};

struct LockFreeType
{
    struct LockFreeTag
    {
    };
};

struct TransparentComparator
{
    using is_transparent = void;
    bool operator()(int a, int b) const { return a < b; }
};

enum class ScopedEnumType
{
    A,
    B
};
enum UnscopedEnumType
{
    X,
    Y
};

struct CustomAllocator
{
    using value_type = int;

    int* allocate(std::size_t n) { return new int[n]; }
    void deallocate(int* p, std::size_t) { delete[] p; }
};

struct FunctionObjectType
{
    int operator()(int x) const { return x * 2; }
};

} // namespace fat_p::testing::concepts

// Provide std::hash specialization
template <>
struct std::hash<fat_p::testing::concepts::HashableType>
{
    std::size_t operator()(const fat_p::testing::concepts::HashableType& h) const noexcept
    {
        return std::hash<int>{}(h.value);
    }
};

namespace fat_p::testing::concepts
{

// =============================================================================
// Container Concept Tests
// =============================================================================

FATP_TEST_CASE(iterable_concept)
{
    // Standard containers
    static_assert(fat_p::concepts::iterable<std::vector<int>>);
    static_assert(fat_p::concepts::iterable<std::list<int>>);
    static_assert(fat_p::concepts::iterable<std::deque<int>>);
    static_assert(fat_p::concepts::iterable<std::set<int>>);
    static_assert(fat_p::concepts::iterable<std::map<int, int>>);
    static_assert(fat_p::concepts::iterable<std::string>);
    static_assert(fat_p::concepts::iterable<std::array<int, 5>>);

    // C arrays
    static_assert(fat_p::concepts::iterable<int[5]>);

    // Custom types
    static_assert(fat_p::concepts::iterable<IterableOnly>);
    static_assert(fat_p::concepts::iterable<ContainerLike>);
    static_assert(!fat_p::concepts::iterable<NotIterable>);
    static_assert(!fat_p::concepts::iterable<SizedOnly>);
    static_assert(!fat_p::concepts::iterable<int>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for iterable concept");
    return true;
}

FATP_TEST_CASE(sized_concept)
{
    static_assert(fat_p::concepts::sized<std::vector<int>>);
    static_assert(fat_p::concepts::sized<std::string>);
    static_assert(fat_p::concepts::sized<std::array<int, 5>>);
    static_assert(fat_p::concepts::sized<SizedOnly>);
    static_assert(fat_p::concepts::sized<ContainerLike>);

    static_assert(!fat_p::concepts::sized<IterableOnly>);
    static_assert(!fat_p::concepts::sized<NotIterable>);
    static_assert(!fat_p::concepts::sized<std::forward_list<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for sized concept");
    return true;
}

FATP_TEST_CASE(container_concept)
{
    static_assert(fat_p::concepts::container<std::vector<int>>);
    static_assert(fat_p::concepts::container<std::list<int>>);
    static_assert(fat_p::concepts::container<std::deque<int>>);
    static_assert(fat_p::concepts::container<std::string>);
    static_assert(fat_p::concepts::container<std::array<int, 5>>);
    static_assert(fat_p::concepts::container<ContainerLike>);

    static_assert(!fat_p::concepts::container<IterableOnly>);
    static_assert(!fat_p::concepts::container<SizedOnly>);
    static_assert(!fat_p::concepts::container<std::forward_list<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for container concept");
    return true;
}

FATP_TEST_CASE(contiguous_container_concept)
{
    static_assert(fat_p::concepts::contiguous_container<std::vector<int>>);
    static_assert(fat_p::concepts::contiguous_container<std::string>);
    static_assert(fat_p::concepts::contiguous_container<std::array<int, 5>>);
    static_assert(fat_p::concepts::contiguous_container<ContiguousLike>);

    static_assert(!fat_p::concepts::contiguous_container<std::list<int>>);
    static_assert(!fat_p::concepts::contiguous_container<std::deque<int>>);
    static_assert(!fat_p::concepts::contiguous_container<ContainerLike>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for contiguous_container concept");
    return true;
}

FATP_TEST_CASE(reservable_concept)
{
    static_assert(fat_p::concepts::reservable<std::vector<int>>);
    static_assert(fat_p::concepts::reservable<std::string>);
    static_assert(fat_p::concepts::reservable<Reservable>);

    static_assert(!fat_p::concepts::reservable<std::list<int>>);
    static_assert(!fat_p::concepts::reservable<std::array<int, 5>>);
    static_assert(!fat_p::concepts::reservable<std::deque<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for reservable concept");
    return true;
}

FATP_TEST_CASE(reverse_iterable_concept)
{
    static_assert(fat_p::concepts::reverse_iterable<std::vector<int>>);
    static_assert(fat_p::concepts::reverse_iterable<std::list<int>>);
    static_assert(fat_p::concepts::reverse_iterable<std::deque<int>>);
    static_assert(fat_p::concepts::reverse_iterable<std::string>);

    static_assert(!fat_p::concepts::reverse_iterable<std::forward_list<int>>);
    static_assert(!fat_p::concepts::reverse_iterable<IterableOnly>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for reverse_iterable concept");
    return true;
}

FATP_TEST_CASE(random_accessible_concept)
{
    static_assert(fat_p::concepts::random_accessible<std::vector<int>>);
    static_assert(fat_p::concepts::random_accessible<std::string>);
    static_assert(fat_p::concepts::random_accessible<std::array<int, 5>>);
    static_assert(fat_p::concepts::random_accessible<std::deque<int>>);

    static_assert(!fat_p::concepts::random_accessible<std::list<int>>);
    static_assert(!fat_p::concepts::random_accessible<std::set<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for random_accessible concept");
    return true;
}

FATP_TEST_CASE(map_like_concept)
{
    static_assert(fat_p::concepts::map_like<std::map<int, int>>);
    static_assert(fat_p::concepts::map_like<std::unordered_map<int, int>>);
    static_assert(fat_p::concepts::map_like<std::multimap<int, int>>);

    static_assert(!fat_p::concepts::map_like<std::vector<int>>);
    static_assert(!fat_p::concepts::map_like<std::set<int>>);
    static_assert(!fat_p::concepts::map_like<std::string>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for map_like concept");
    return true;
}

// =============================================================================
// Comparison Concept Tests
// =============================================================================

FATP_TEST_CASE(hashable_concept)
{
    static_assert(fat_p::concepts::hashable<int>);
    static_assert(fat_p::concepts::hashable<std::string>);
    static_assert(fat_p::concepts::hashable<HashableType>);

    static_assert(!fat_p::concepts::hashable<NotHashable>);
    static_assert(!fat_p::concepts::hashable<std::vector<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for hashable concept");
    return true;
}

FATP_TEST_CASE(equality_comparable_concept)
{
    static_assert(fat_p::concepts::equality_comparable<int>);
    static_assert(fat_p::concepts::equality_comparable<std::string>);
    static_assert(fat_p::concepts::equality_comparable<std::vector<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for equality_comparable concept");
    return true;
}

FATP_TEST_CASE(totally_ordered_concept)
{
    static_assert(fat_p::concepts::totally_ordered<int>);
    static_assert(fat_p::concepts::totally_ordered<std::string>);
    static_assert(fat_p::concepts::totally_ordered<double>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for totally_ordered concept");
    return true;
}

FATP_TEST_CASE(three_way_comparable_concept)
{
    static_assert(fat_p::concepts::three_way_comparable<int>);
    static_assert(fat_p::concepts::three_way_comparable<std::string>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for three_way_comparable concept");
    return true;
}

FATP_TEST_CASE(valid_comparator_concept)
{
    static_assert(fat_p::concepts::valid_comparator<std::less<int>, int>);
    static_assert(fat_p::concepts::valid_comparator<std::greater<int>, int>);
    static_assert(fat_p::concepts::valid_comparator<TransparentComparator, int>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for valid_comparator concept");
    return true;
}

FATP_TEST_CASE(transparent_concept)
{
    static_assert(fat_p::concepts::transparent<TransparentComparator>);
    static_assert(fat_p::concepts::transparent<std::less<>>);
    static_assert(fat_p::concepts::transparent<std::greater<>>);

    static_assert(!fat_p::concepts::transparent<std::less<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for transparent concept");
    return true;
}

// =============================================================================
// Stream Concept Tests
// =============================================================================

FATP_TEST_CASE(streamable_concept)
{
    static_assert(fat_p::concepts::streamable<int>);
    static_assert(fat_p::concepts::streamable<std::string>);
    static_assert(fat_p::concepts::streamable<double>);
    static_assert(fat_p::concepts::streamable<Streamable>);

    static_assert(!fat_p::concepts::streamable<NotStreamable>);
    static_assert(!fat_p::concepts::streamable<std::vector<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for streamable concept");
    return true;
}

// =============================================================================
// Callable Concept Tests
// =============================================================================

FATP_TEST_CASE(invocable_concept)
{
    static_assert(fat_p::concepts::invocable<void()>);
    static_assert(fat_p::concepts::invocable<int(int), int>);
    static_assert(fat_p::concepts::invocable<FunctionObjectType, int>);

    auto lambda = [](int x) { return x * 2; };
    static_assert(fat_p::concepts::invocable<decltype(lambda), int>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for invocable concept");
    return true;
}

FATP_TEST_CASE(invocable_r_concept)
{
    static_assert(fat_p::concepts::invocable_r<int, int(int), int>);
    static_assert(fat_p::concepts::invocable_r<int, FunctionObjectType, int>);

    static_assert(!fat_p::concepts::invocable_r<std::string, int(int), int>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for invocable_r concept");
    return true;
}

FATP_TEST_CASE(function_object_concept)
{
    static_assert(fat_p::concepts::function_object<FunctionObjectType>);
    static_assert(fat_p::concepts::function_object<std::less<int>>);

    auto lambda = [](int x) { return x; };
    static_assert(fat_p::concepts::function_object<decltype(lambda)>);

    static_assert(!fat_p::concepts::function_object<int>);
    static_assert(!fat_p::concepts::function_object<void (*)()>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for function_object concept");
    return true;
}

// =============================================================================
// Type Classification Concept Tests
// =============================================================================

FATP_TEST_CASE(atomic_type_concept)
{
    static_assert(fat_p::concepts::atomic_type<std::atomic<int>>);
    static_assert(fat_p::concepts::atomic_type<std::atomic<bool>>);

    static_assert(!fat_p::concepts::atomic_type<int>);
    static_assert(!fat_p::concepts::atomic_type<std::vector<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for atomic_type concept");
    return true;
}

FATP_TEST_CASE(scoped_enum_concept)
{
    static_assert(fat_p::concepts::scoped_enum<ScopedEnumType>);

    static_assert(!fat_p::concepts::scoped_enum<UnscopedEnumType>);
    static_assert(!fat_p::concepts::scoped_enum<int>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for scoped_enum concept");
    return true;
}

FATP_TEST_CASE(allocator_concept)
{
    static_assert(fat_p::concepts::allocator<std::allocator<int>>);
    static_assert(fat_p::concepts::allocator<CustomAllocator>);

    static_assert(!fat_p::concepts::allocator<int>);
    static_assert(!fat_p::concepts::allocator<std::vector<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for allocator concept");
    return true;
}

// =============================================================================
// Serialization Concept Tests
// =============================================================================

FATP_TEST_CASE(serializable_concept)
{
    static_assert(fat_p::concepts::has_serialize<SerializableType>);
    static_assert(fat_p::concepts::has_deserialize<SerializableType>);
    static_assert(fat_p::concepts::serializable<SerializableType>);

    static_assert(!fat_p::concepts::serializable<int>);
    static_assert(!fat_p::concepts::serializable<std::vector<int>>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for serializable concept");
    return true;
}

// =============================================================================
// Policy Concept Tests
// =============================================================================

FATP_TEST_CASE(policy_concepts)
{
    static_assert(fat_p::concepts::has_validate<HasValidate>);
    static_assert(!fat_p::concepts::has_validate<int>);

    static_assert(fat_p::concepts::has_shared_locking<HasSharedGuard>);
    static_assert(!fat_p::concepts::has_shared_locking<int>);

    static_assert(fat_p::concepts::lock_free_policy<LockFreeType>);
    static_assert(!fat_p::concepts::lock_free_policy<int>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for policy concepts");
    return true;
}

// =============================================================================
// Array and Tuple Concept Tests
// =============================================================================

FATP_TEST_CASE(array_concepts)
{
    static_assert(fat_p::concepts::bounded_array<int[5]>);
    static_assert(!fat_p::concepts::bounded_array<int[]>);
    static_assert(!fat_p::concepts::bounded_array<int>);

    static_assert(fat_p::concepts::unbounded_array<int[]>);
    static_assert(!fat_p::concepts::unbounded_array<int[5]>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for array concepts");
    return true;
}

FATP_TEST_CASE(tuple_like_concept)
{
    static_assert(fat_p::concepts::tuple_like<std::tuple<int, double>>);
    static_assert(fat_p::concepts::tuple_like<std::pair<int, int>>);
    static_assert(fat_p::concepts::tuple_like<std::array<int, 5>>);

    static_assert(!fat_p::concepts::tuple_like<std::vector<int>>);
    static_assert(!fat_p::concepts::tuple_like<int>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for tuple_like concept");
    return true;
}

FATP_TEST_CASE(aggregate_concept)
{
    struct AggregateType
    {
        int x;
        double y;
    };

    static_assert(fat_p::concepts::aggregate<AggregateType>);

    struct NonAggregate
    {
        NonAggregate() {}
        int x;
    };

    static_assert(!fat_p::concepts::aggregate<NonAggregate>);

    FATP_ASSERT_TRUE(true, "Static asserts passed for aggregate concept");
    return true;
}

} // namespace fat_p::testing::concepts

// =============================================================================
// Public Interface
// =============================================================================

namespace fat_p::testing
{


inline void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_Concepts()
{
    FATP_PRINT_HEADER(CONCEPTS)

    TestRunner runner;
    auto& out = *get_test_config().output;

    // Container Concepts
    out << colors::blue() << "--- Container Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, concepts, iterable_concept);
    FATP_RUN_TEST_NS(runner, concepts, sized_concept);
    FATP_RUN_TEST_NS(runner, concepts, container_concept);
    FATP_RUN_TEST_NS(runner, concepts, contiguous_container_concept);
    FATP_RUN_TEST_NS(runner, concepts, reservable_concept);
    FATP_RUN_TEST_NS(runner, concepts, reverse_iterable_concept);
    FATP_RUN_TEST_NS(runner, concepts, random_accessible_concept);
    FATP_RUN_TEST_NS(runner, concepts, map_like_concept);

    // Comparison Concepts
    out << "\n" << colors::blue() << "--- Comparison Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, concepts, hashable_concept);
    FATP_RUN_TEST_NS(runner, concepts, equality_comparable_concept);
    FATP_RUN_TEST_NS(runner, concepts, totally_ordered_concept);
    FATP_RUN_TEST_NS(runner, concepts, three_way_comparable_concept);
    FATP_RUN_TEST_NS(runner, concepts, valid_comparator_concept);
    FATP_RUN_TEST_NS(runner, concepts, transparent_concept);

    // Stream Concepts
    out << "\n" << colors::blue() << "--- Stream Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, concepts, streamable_concept);

    // Callable Concepts
    out << "\n" << colors::blue() << "--- Callable Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, concepts, invocable_concept);
    FATP_RUN_TEST_NS(runner, concepts, invocable_r_concept);
    FATP_RUN_TEST_NS(runner, concepts, function_object_concept);

    // Type Classification Concepts
    out << "\n" << colors::blue() << "--- Type Classification Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, concepts, atomic_type_concept);
    FATP_RUN_TEST_NS(runner, concepts, scoped_enum_concept);
    FATP_RUN_TEST_NS(runner, concepts, allocator_concept);

    // Serialization Concepts
    out << "\n" << colors::blue() << "--- Serialization Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, concepts, serializable_concept);

    // Policy Concepts
    out << "\n" << colors::blue() << "--- Policy Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, concepts, policy_concepts);

    // Array and Tuple Concepts
    out << "\n" << colors::blue() << "--- Array and Tuple Concepts ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, concepts, array_concepts);
    FATP_RUN_TEST_NS(runner, concepts, tuple_like_concept);
    FATP_RUN_TEST_NS(runner, concepts, aggregate_concept);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Concepts() ? 0 : 1;
}
#endif
