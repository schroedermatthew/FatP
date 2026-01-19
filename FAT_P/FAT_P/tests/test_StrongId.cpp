/**
 * @file test_StrongId.cpp
 * @brief Comprehensive unit tests for StrongId template
 *
 * Tests cover:
 * - Basic functionality (construction, accessors, comparison)
 * - Arithmetic operations (all operators)
 * - Bitwise operations
 * - CheckPolicy validation
 * - Expected-based safe creation
 * - Swap functionality
 * - Hash support for containers
 * - AtomicStrongId usage
 * - Zero-overhead sanity benchmark
 */
/*
FATP_META:
  meta_version: 1
  component: StrongId
  file_role: test
  path: tests/test_StrongId.cpp
  namespace: fat_p
  summary: "Unit tests for StrongId."
  related:
    docs:
      - Documentation/IN WORK/Overview - StrongId.md
      - Documentation/IN WORK/User Manual - StrongId.md
      - Documentation/IN WORK/Companion Guide - StrongId.md
    headers:
      - fat_p/CppStandardDetection.h
      - fat_p/StrongId.h
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

#include "CppStandardDetection.h"
#include "FatPTest.h"
#include "StrongId.h"
#include <atomic>
#include <limits>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fat_p::testing::strongid
{

// --- Test Tags ---
struct UserIdTag
{
};
struct TransactionIdTag
{
};
struct ProductIdTag
{
};
struct UncheckedIdTag
{
};

// --- Type Aliases (4-parameter StrongId) ---
using UserId = StrongId<int, UserIdTag>;
using TransactionId = StrongId<long, TransactionIdTag>;
using ProductId = StrongId<int, ProductIdTag, strong_id::PositiveCheckPolicy>;

// Unchecked version for performance comparison
using UncheckedId = StrongId<int, UncheckedIdTag, strong_id::NoCheckPolicy, strong_id::UncheckedOpPolicy>;

// Thread-safe version using std::atomic
using AtomicUserId = AtomicStrongId<int, UserIdTag>;

// =============================================================================
// Basic Functionality Tests
// =============================================================================

FATP_TEST_CASE(default_constructor)
{
    UserId id;
    FATP_ASSERT_EQ(id.get(), 0, "Default constructor should initialize to 0");
    return true;
}

FATP_TEST_CASE(explicit_constructor)
{
    UserId id(42);
    FATP_ASSERT_EQ(id.get(), 42, "Explicit constructor should set value");
    return true;
}

FATP_TEST_CASE(default_constructor_with_check_policy)
{
    // Default PositiveCheckPolicy allows 0
    ProductId id;
    FATP_ASSERT_EQ(id.get(), 0, "Default constructor should work with check policy");
    return true;
}

FATP_TEST_CASE(type_safety)
{
    UserId user_id(100);
    TransactionId trans_id(100);

    // These should not compile (different types):
    // user_id = trans_id;  // Error: different types
    // bool eq = (user_id == trans_id);  // Error: different types

    // But same type comparisons work:
    UserId another_user(100);
    FATP_ASSERT_TRUE(user_id == another_user, "Same type comparison should work");
    return true;
}

FATP_TEST_CASE(get_accessor)
{
    UserId id(123);
    FATP_ASSERT_EQ(id.get(), 123, "get() should return underlying value");
    return true;
}

FATP_TEST_CASE(value_accessor)
{
    UserId id(456);
    FATP_ASSERT_EQ(id.value(), 456, "value() should return underlying value");
    return true;
}

FATP_TEST_CASE(explicit_cast)
{
    UserId id(456);
    int value = static_cast<int>(id);
    FATP_ASSERT_EQ(value, 456, "Explicit cast should work");
    return true;
}

// =============================================================================
// Comparison Operator Tests
// =============================================================================

FATP_TEST_CASE(equality_comparison)
{
    UserId id1(100);
    UserId id2(100);
    UserId id3(200);

    FATP_ASSERT_TRUE(id1 == id2, "Equal IDs should compare equal");
    FATP_ASSERT_FALSE(id1 == id3, "Unequal IDs should not compare equal");
    return true;
}

FATP_TEST_CASE(inequality_comparison)
{
    UserId id1(100);
    UserId id2(200);

    FATP_ASSERT_TRUE(id1 != id2, "Unequal IDs should compare not equal");
    FATP_ASSERT_FALSE(id1 != id1, "Same ID should not compare not equal");
    return true;
}

FATP_TEST_CASE(less_than_comparison)
{
    UserId id1(100);
    UserId id2(200);

    FATP_ASSERT_TRUE(id1 < id2, "Smaller ID should be less than larger");
    FATP_ASSERT_FALSE(id2 < id1, "Larger ID should not be less than smaller");
    FATP_ASSERT_FALSE(id1 < id1, "ID should not be less than itself");
    return true;
}

FATP_TEST_CASE(all_relational_operators)
{
    UserId id1(100);
    UserId id2(200);

    FATP_ASSERT_TRUE(id1 <= id2, "Less-or-equal should work");
    FATP_ASSERT_TRUE(id1 <= id1, "Equal IDs should satisfy <=");
    FATP_ASSERT_TRUE(id2 > id1, "Greater should work");
    FATP_ASSERT_TRUE(id2 >= id1, "Greater-or-equal should work");
    FATP_ASSERT_TRUE(id1 >= id1, "Equal IDs should satisfy >=");
    return true;
}

#if FATP_HAS_CPP20
FATP_TEST_CASE(spaceship_operator)
{
    UserId id1(100);
    UserId id2(200);
    UserId id3(100);

    FATP_ASSERT_TRUE((id1 <=> id2) < 0, "Spaceship operator: less than");
    FATP_ASSERT_TRUE((id2 <=> id1) > 0, "Spaceship operator: greater than");
    FATP_ASSERT_TRUE((id1 <=> id3) == 0, "Spaceship operator: equal");
    return true;
}
#endif

// =============================================================================
// Arithmetic Operator Tests
// =============================================================================

FATP_TEST_CASE(increment_operators)
{
    UserId id(10);

    ++id;
    FATP_ASSERT_EQ(id.get(), 11, "Pre-increment should work");

    UserId id2 = id++;
    FATP_ASSERT_EQ(id.get(), 12, "Post-increment should increment");
    FATP_ASSERT_EQ(id2.get(), 11, "Post-increment should return old value");
    return true;
}

FATP_TEST_CASE(decrement_operators)
{
    UserId id(10);

    --id;
    FATP_ASSERT_EQ(id.get(), 9, "Pre-decrement should work");

    UserId id2 = id--;
    FATP_ASSERT_EQ(id.get(), 8, "Post-decrement should decrement");
    FATP_ASSERT_EQ(id2.get(), 9, "Post-decrement should return old value");
    return true;
}

FATP_TEST_CASE(addition_operators)
{
    UserId id(10);

    UserId id2 = id + 5;
    FATP_ASSERT_EQ(id2.get(), 15, "Addition should work");

    id += 3;
    FATP_ASSERT_EQ(id.get(), 13, "Compound addition should work");
    return true;
}

FATP_TEST_CASE(subtraction_operators)
{
    UserId id(10);

    UserId id2 = id - 3;
    FATP_ASSERT_EQ(id2.get(), 7, "Subtraction should work");

    id -= 2;
    FATP_ASSERT_EQ(id.get(), 8, "Compound subtraction should work");
    return true;
}

FATP_TEST_CASE(multiplication_operators)
{
    UserId id(10);

    UserId id2 = id * 3;
    FATP_ASSERT_EQ(id2.get(), 30, "Multiplication should work");

    id *= 2;
    FATP_ASSERT_EQ(id.get(), 20, "Compound multiplication should work");
    return true;
}

FATP_TEST_CASE(division_operators)
{
    UserId id(20);

    UserId id2 = id / 4;
    FATP_ASSERT_EQ(id2.get(), 5, "Division should work");

    id /= 2;
    FATP_ASSERT_EQ(id.get(), 10, "Compound division should work");
    return true;
}

FATP_TEST_CASE(modulo_operators)
{
    UserId id(17);

    UserId id2 = id % 5;
    FATP_ASSERT_EQ(id2.get(), 2, "Modulo should work");

    id %= 7;
    FATP_ASSERT_EQ(id.get(), 3, "Compound modulo should work");
    return true;
}

FATP_TEST_CASE(unary_operators)
{
    UserId id(42);

    UserId pos = +id;
    FATP_ASSERT_EQ(pos.get(), 42, "Unary plus should work");

    UserId neg = -id;
    FATP_ASSERT_EQ(neg.get(), -42, "Unary minus should work");
    return true;
}

// =============================================================================
// Bitwise Operator Tests
// =============================================================================

FATP_TEST_CASE(bitwise_and)
{
    UserId id(0b1100);
    UserId result = id & 0b1010;
    FATP_ASSERT_EQ(result.get(), 0b1000, "Bitwise AND should work");

    id &= 0b1110;
    FATP_ASSERT_EQ(id.get(), 0b1100, "Compound AND should work");
    return true;
}

FATP_TEST_CASE(bitwise_or)
{
    UserId id(0b1100);
    UserId result = id | 0b0011;
    FATP_ASSERT_EQ(result.get(), 0b1111, "Bitwise OR should work");

    id |= 0b0001;
    FATP_ASSERT_EQ(id.get(), 0b1101, "Compound OR should work");
    return true;
}

FATP_TEST_CASE(bitwise_xor)
{
    UserId id(0b1100);
    UserId result = id ^ 0b1010;
    FATP_ASSERT_EQ(result.get(), 0b0110, "Bitwise XOR should work");

    id ^= 0b0011;
    FATP_ASSERT_EQ(id.get(), 0b1111, "Compound XOR should work");
    return true;
}

FATP_TEST_CASE(bitwise_not)
{
    UserId id(0);
    UserId result = ~id;
    FATP_ASSERT_EQ(result.get(), ~0, "Bitwise NOT should work");
    return true;
}

FATP_TEST_CASE(bit_shifts)
{
    UserId id(1);

    UserId left = id << 4;
    FATP_ASSERT_EQ(left.get(), 16, "Left shift should work");

    UserId right = left >> 2;
    FATP_ASSERT_EQ(right.get(), 4, "Right shift should work");

    id <<= 3;
    FATP_ASSERT_EQ(id.get(), 8, "Compound left shift should work");

    id >>= 1;
    FATP_ASSERT_EQ(id.get(), 4, "Compound right shift should work");
    return true;
}

// =============================================================================
// CheckPolicy Validation Tests
// =============================================================================

FATP_TEST_CASE(positive_check_policy_valid)
{
    ProductId id(42);
    FATP_ASSERT_EQ(id.get(), 42, "Positive value should be allowed");

    ProductId zero(0);
    FATP_ASSERT_EQ(zero.get(), 0, "Zero should be allowed with PositiveCheckPolicy");
    return true;
}

FATP_TEST_CASE(positive_check_policy_invalid)
{
    bool caught = false;
    try
    {
        ProductId id(-1);
    }
    catch (const std::invalid_argument&)
    {
        caught = true;
    }
    FATP_ASSERT_TRUE(caught, "Negative value should throw with PositiveCheckPolicy");
    return true;
}

FATP_TEST_CASE(check_policy_in_default_constructor)
{
    // PositiveCheckPolicy allows 0, so default construction should work
    ProductId id;
    FATP_ASSERT_EQ(id.get(), 0, "Default construction should work with policy that allows 0");
    return true;
}

// =============================================================================
// Expected-Based Safe Creation Tests
// =============================================================================

FATP_TEST_CASE(expected_create_success)
{
    auto result = ProductId::create(42);
    FATP_ASSERT_TRUE(result.has_value(), "create() should succeed for valid value");
    FATP_ASSERT_EQ(result->get(), 42, "Created ID should have correct value");
    return true;
}

FATP_TEST_CASE(expected_create_failure)
{
    auto result = ProductId::create(-1);
    FATP_ASSERT_FALSE(result.has_value(), "create() should fail for invalid value");
    FATP_ASSERT_TRUE(result.error().find("negative") != std::string::npos || result.error().size() > 0,
                     "Error message should be present");
    return true;
}

// =============================================================================
// Assignment Operator Tests
// =============================================================================

FATP_TEST_CASE(copy_assignment)
{
    UserId id1(100);
    UserId id2(200);

    id2 = id1;
    FATP_ASSERT_EQ(id2.get(), 100, "Copy assignment should work");
    return true;
}

FATP_TEST_CASE(move_assignment)
{
    UserId id1(100);
    UserId id2(200);

    id2 = std::move(id1);
    FATP_ASSERT_EQ(id2.get(), 100, "Move assignment should work");
    return true;
}

FATP_TEST_CASE(self_assignment)
{
    UserId id(100);
    id = id;
    FATP_ASSERT_EQ(id.get(), 100, "Self-assignment should be safe");
    return true;
}

// =============================================================================
// Swap Functionality Tests
// =============================================================================

FATP_TEST_CASE(member_swap)
{
    UserId id1(100);
    UserId id2(200);

    id1.swap(id2);

    FATP_ASSERT_EQ(id1.get(), 200, "After swap, id1 should have id2's value");
    FATP_ASSERT_EQ(id2.get(), 100, "After swap, id2 should have id1's value");
    return true;
}

FATP_TEST_CASE(adl_swap)
{
    UserId id1(100);
    UserId id2(200);

    using std::swap;
    swap(id1, id2);

    FATP_ASSERT_EQ(id1.get(), 200, "ADL swap should work");
    FATP_ASSERT_EQ(id2.get(), 100, "ADL swap should work");
    return true;
}

// =============================================================================
// Hash and Container Tests
// =============================================================================

FATP_TEST_CASE(hash_function)
{
    std::hash<UserId> hasher;
    UserId id1(42);
    UserId id2(42);
    UserId id3(100);

    FATP_ASSERT_EQ(hasher(id1), hasher(id2), "Same values should have same hash");
    // Note: Different values may have same hash (collision), so we don't test inequality
    (void)hasher(id3); // Just ensure it compiles and runs
    return true;
}

FATP_TEST_CASE(unordered_map_usage)
{
    std::unordered_map<UserId, std::string> map;
    map[UserId(1)] = "one";
    map[UserId(2)] = "two";

    FATP_ASSERT_EQ(map[UserId(1)], "one", "Lookup should work");
    FATP_ASSERT_EQ(map[UserId(2)], "two", "Lookup should work");
    return true;
}

// =============================================================================
// Atomic StrongId Tests
// =============================================================================

FATP_TEST_CASE(atomic_basic_operations)
{
    AtomicUserId atomic_id(UserId(42));

    UserId loaded = atomic_id.load();
    FATP_ASSERT_EQ(loaded.get(), 42, "Atomic load should work");

    atomic_id.store(UserId(100));
    FATP_ASSERT_EQ(atomic_id.load().get(), 100, "Atomic store should work");
    return true;
}

FATP_TEST_CASE(atomic_exchange)
{
    AtomicUserId atomic_id(UserId(42));

    UserId old = atomic_id.exchange(UserId(100));
    FATP_ASSERT_EQ(old.get(), 42, "Exchange should return old value");
    FATP_ASSERT_EQ(atomic_id.load().get(), 100, "Exchange should set new value");
    return true;
}

FATP_TEST_CASE(atomic_compare_exchange)
{
    AtomicUserId atomic_id(UserId(42));

    UserId expected(42);
    bool success = atomic_id.compare_exchange_strong(expected, UserId(100));
    FATP_ASSERT_TRUE(success, "CAS should succeed when expected matches");
    FATP_ASSERT_EQ(atomic_id.load().get(), 100, "CAS should set new value on success");

    expected = UserId(42); // Wrong expected value
    success = atomic_id.compare_exchange_strong(expected, UserId(200));
    FATP_ASSERT_FALSE(success, "CAS should fail when expected doesn't match");
    FATP_ASSERT_EQ(expected.get(), 100, "CAS should update expected on failure");
    return true;
}

FATP_TEST_CASE(atomic_concurrent_reads)
{
    AtomicUserId atomic_id(UserId(42));
    std::atomic<bool> all_correct{true};

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < 1000; ++j)
            {
                UserId val = atomic_id.load();
                if (val.get() != 42)
                {
                    all_correct = false;
                }
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_TRUE(all_correct.load(), "Concurrent reads should be consistent");
    return true;
}

FATP_TEST_CASE(atomic_concurrent_increments)
{
    AtomicUserId atomic_id(UserId(0));
    constexpr int threads_count = 4;
    constexpr int increments_per_thread = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < threads_count; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < increments_per_thread; ++j)
            {
                atomic_id.fetch_add(1);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_EQ(atomic_id.load().get(), threads_count * increments_per_thread,
                   "Concurrent increments should be atomic");
    return true;
}

// =============================================================================
// Type Traits Tests
// =============================================================================

FATP_TEST_CASE(is_strong_id_trait)
{
    static_assert(is_strong_id_v<UserId>, "UserId should be a StrongId");
    static_assert(is_strong_id_v<ProductId>, "ProductId should be a StrongId");
    static_assert(!is_strong_id_v<int>, "int should not be a StrongId");
    static_assert(!is_strong_id_v<std::string>, "string should not be a StrongId");
    return true;
}

// =============================================================================
// Sentinel/Validity Pattern Tests
// =============================================================================

FATP_TEST_CASE(invalid_sentinel)
{
    UserId invalid = UserId::invalid();
    FATP_ASSERT_EQ(invalid.get(), std::numeric_limits<int>::max(), "invalid() should return max sentinel");
    return true;
}

FATP_TEST_CASE(is_valid_check)
{
    UserId valid(42);
    UserId invalid = UserId::invalid();

    FATP_ASSERT_TRUE(valid.isValid(), "Regular ID should be valid");
    FATP_ASSERT_FALSE(invalid.isValid(), "Invalid sentinel should not be valid");
    return true;
}

FATP_TEST_CASE(min_max_methods)
{
    UserId min_id = UserId::min();
    UserId max_id = UserId::max();

    FATP_ASSERT_EQ(min_id.get(), std::numeric_limits<int>::min(), "min() should return min value");
    // max() returns max-1 to reserve max for invalid sentinel
    FATP_ASSERT_EQ(max_id.get(), std::numeric_limits<int>::max() - 1, "max() should return max-1");
    return true;
}

// =============================================================================
// New Check Policy Tests
// =============================================================================

// NonZeroCheckPolicy tests
struct NonZeroIdTag
{
};
using NonZeroId = StrongId<int, NonZeroIdTag, strong_id::NonZeroCheckPolicy>;

FATP_TEST_CASE(non_zero_policy_sentinel_factories_bypass_validation)
{
    NonZeroId invalid = NonZeroId::invalid();
    FATP_ASSERT_EQ(invalid.get(), std::numeric_limits<int>::max(), "invalid() should return max sentinel");
    FATP_ASSERT_FALSE(invalid.isValid(), "Invalid sentinel should not be valid");

    NonZeroId maxId = NonZeroId::max();
    FATP_ASSERT_TRUE(maxId.isValid(), "max() should not return the invalid sentinel");
    return true;
}

FATP_TEST_CASE(range_policy_sentinel_factories_bypass_validation)
{
    struct RangeIdTag
    {
    };
    using RangeId = StrongId<int, RangeIdTag, strong_id::RangeCheckPolicy<1, 10>>;

    RangeId invalid = RangeId::invalid();
    FATP_ASSERT_EQ(invalid.get(), std::numeric_limits<int>::max(), "invalid() should return max sentinel");

    RangeId minId = RangeId::min();
    FATP_ASSERT_EQ(minId.get(), std::numeric_limits<int>::min(), "min() should return underlying min");

    RangeId maxId = RangeId::max();
    FATP_ASSERT_EQ(maxId.get(),
                   std::numeric_limits<int>::max() - 1,
                   "max() should return the maximum non-sentinel value");
    return true;
}

FATP_TEST_CASE(non_zero_policy_valid)
{
    NonZeroId id(42);
    FATP_ASSERT_EQ(id.get(), 42, "Non-zero value should be allowed");

    NonZeroId negId(-5);
    FATP_ASSERT_EQ(negId.get(), -5, "Negative non-zero value should be allowed");
    return true;
}

FATP_TEST_CASE(non_zero_policy_invalid)
{
    bool caught = false;
    try
    {
        NonZeroId id(0);
    }
    catch (const std::invalid_argument&)
    {
        caught = true;
    }
    FATP_ASSERT_TRUE(caught, "Zero value should throw with NonZeroCheckPolicy");
    return true;
}

// StrictlyPositiveCheckPolicy tests
struct StrictlyPositiveIdTag
{
};
using StrictlyPositiveId = StrongId<int, StrictlyPositiveIdTag, strong_id::StrictlyPositiveCheckPolicy>;

FATP_TEST_CASE(strictly_positive_policy_valid)
{
    StrictlyPositiveId id(42);
    FATP_ASSERT_EQ(id.get(), 42, "Positive value should be allowed");
    return true;
}

FATP_TEST_CASE(strictly_positive_policy_zero_invalid)
{
    bool caught = false;
    try
    {
        StrictlyPositiveId id(0);
    }
    catch (const std::invalid_argument&)
    {
        caught = true;
    }
    FATP_ASSERT_TRUE(caught, "Zero should throw with StrictlyPositiveCheckPolicy");
    return true;
}

FATP_TEST_CASE(strictly_positive_policy_negative_invalid)
{
    bool caught = false;
    try
    {
        StrictlyPositiveId id(-5);
    }
    catch (const std::invalid_argument&)
    {
        caught = true;
    }
    FATP_ASSERT_TRUE(caught, "Negative should throw with StrictlyPositiveCheckPolicy");
    return true;
}

// RangeCheckPolicy tests
struct RangeIdTag
{
};
using RangeId = StrongId<int, RangeIdTag, strong_id::RangeCheckPolicy<1, 100>>;

FATP_TEST_CASE(range_policy_valid)
{
    RangeId idMin(1);
    RangeId idMid(50);
    RangeId idMax(100);

    FATP_ASSERT_EQ(idMin.get(), 1, "Min boundary should be allowed");
    FATP_ASSERT_EQ(idMid.get(), 50, "Middle value should be allowed");
    FATP_ASSERT_EQ(idMax.get(), 100, "Max boundary should be allowed");
    return true;
}

FATP_TEST_CASE(range_policy_below_min_invalid)
{
    bool caught = false;
    try
    {
        RangeId id(0);
    }
    catch (const std::out_of_range&)
    {
        caught = true;
    }
    FATP_ASSERT_TRUE(caught, "Value below min should throw");
    return true;
}

FATP_TEST_CASE(range_policy_above_max_invalid)
{
    bool caught = false;
    try
    {
        RangeId id(101);
    }
    catch (const std::out_of_range&)
    {
        caught = true;
    }
    FATP_ASSERT_TRUE(caught, "Value above max should throw");
    return true;
}

// =============================================================================
// StrongId-to-StrongId Bitwise Operations Tests
// =============================================================================

FATP_TEST_CASE(bitwise_and_strongid)
{
    UserId id1(0b1100);
    UserId id2(0b1010);
    UserId result = id1 & id2;
    FATP_ASSERT_EQ(result.get(), 0b1000, "StrongId & StrongId should work");
    return true;
}

FATP_TEST_CASE(bitwise_or_strongid)
{
    UserId id1(0b1100);
    UserId id2(0b0011);
    UserId result = id1 | id2;
    FATP_ASSERT_EQ(result.get(), 0b1111, "StrongId | StrongId should work");
    return true;
}

FATP_TEST_CASE(bitwise_xor_strongid)
{
    UserId id1(0b1100);
    UserId id2(0b1010);
    UserId result = id1 ^ id2;
    FATP_ASSERT_EQ(result.get(), 0b0110, "StrongId ^ StrongId should work");
    return true;
}

// =============================================================================
// Zero-Overhead Sanity Benchmark
// =============================================================================

/**
 * @brief Single sanity benchmark validating zero-overhead abstraction.
 *
 * Compares UncheckedStrongId comparison vs raw int comparison.
 * A ratio of ~1.0x confirms the wrapper compiles away completely.
 *
 * We use comparison (operator<) because:
 * - It's the most fundamental operation for containers and sorting
 * - It compiles to a single CPU instruction
 * - It clearly shows whether the wrapper adds any overhead
 */
void run_zero_overhead_sanity_benchmark()
{
    auto& out = *get_test_config().output;

    out << "\n"
        << colors::cyan() << colors::bold()
        << "=== Zero-Overhead Sanity Benchmark ===" << colors::reset() << "\n\n";

    out << "Comparing: UncheckedStrongId vs raw int (operator<)\n"
        << "A ratio near 1.0x confirms the wrapper compiles away completely.\n\n";

    UncheckedId strongid_a(42);
    UncheckedId strongid_b(100);
    int raw_a = 42;
    int raw_b = 100;

    // Benchmark StrongId comparison
    benchmark("UncheckedStrongId operator<", [&strongid_a, &strongid_b]() {
        volatile bool r = strongid_a < strongid_b;
        (void)r;
    });

    // Benchmark raw int comparison
    benchmark("Raw int operator<", [&raw_a, &raw_b]() {
        volatile bool r = raw_a < raw_b;
        (void)r;
    });

    out << "\n" << colors::green()
        << "Zero overhead confirmed if both times are nearly identical."
        << colors::reset() << "\n\n";
}

} // namespace fat_p::testing::strongid

namespace fat_p::testing
{

bool test_StrongId()
{
    FATP_PRINT_HEADER(STRONG ID)

    TestRunner runner;

    auto& config = get_test_config();
    config.verbose = true;

    auto& out = *config.output;
    // Basic Functionality
    out << colors::blue() << "--- Basic Functionality ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, default_constructor);
    FATP_RUN_TEST_NS(runner, strongid, explicit_constructor);
    FATP_RUN_TEST_NS(runner, strongid, default_constructor_with_check_policy);
    FATP_RUN_TEST_NS(runner, strongid, type_safety);
    FATP_RUN_TEST_NS(runner, strongid, get_accessor);
    FATP_RUN_TEST_NS(runner, strongid, value_accessor);
    FATP_RUN_TEST_NS(runner, strongid, explicit_cast);

    // Comparison Operators
    out << "\n" << colors::blue() << "--- Comparison Operators ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, equality_comparison);
    FATP_RUN_TEST_NS(runner, strongid, inequality_comparison);
    FATP_RUN_TEST_NS(runner, strongid, less_than_comparison);
    FATP_RUN_TEST_NS(runner, strongid, all_relational_operators);
#if FATP_HAS_CPP20
    FATP_RUN_TEST_NS(runner, strongid, spaceship_operator);
#endif

    // Arithmetic Operators
    out << "\n" << colors::blue() << "--- Arithmetic Operators ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, increment_operators);
    FATP_RUN_TEST_NS(runner, strongid, decrement_operators);
    FATP_RUN_TEST_NS(runner, strongid, addition_operators);
    FATP_RUN_TEST_NS(runner, strongid, subtraction_operators);
    FATP_RUN_TEST_NS(runner, strongid, multiplication_operators);
    FATP_RUN_TEST_NS(runner, strongid, division_operators);
    FATP_RUN_TEST_NS(runner, strongid, modulo_operators);
    FATP_RUN_TEST_NS(runner, strongid, unary_operators);

    // Bitwise Operators
    out << "\n" << colors::blue() << "--- Bitwise Operators ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, bitwise_and);
    FATP_RUN_TEST_NS(runner, strongid, bitwise_or);
    FATP_RUN_TEST_NS(runner, strongid, bitwise_xor);
    FATP_RUN_TEST_NS(runner, strongid, bitwise_not);
    FATP_RUN_TEST_NS(runner, strongid, bit_shifts);

    // CheckPolicy
    out << "\n" << colors::blue() << "--- CheckPolicy Validation ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, positive_check_policy_valid);
    FATP_RUN_TEST_NS(runner, strongid, positive_check_policy_invalid);
    FATP_RUN_TEST_NS(runner, strongid, check_policy_in_default_constructor);

    // Expected
    out << "\n" << colors::blue() << "--- Expected-Based Safe Creation ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, expected_create_success);
    FATP_RUN_TEST_NS(runner, strongid, expected_create_failure);

    // Assignment
    out << "\n" << colors::blue() << "--- Assignment Operators ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, copy_assignment);
    FATP_RUN_TEST_NS(runner, strongid, move_assignment);
    FATP_RUN_TEST_NS(runner, strongid, self_assignment);

    // Swap
    out << "\n" << colors::blue() << "--- Swap Functionality ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, member_swap);
    FATP_RUN_TEST_NS(runner, strongid, adl_swap);

    // Hash and Containers
    out << "\n" << colors::blue() << "--- Hash and Containers ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, hash_function);
    FATP_RUN_TEST_NS(runner, strongid, unordered_map_usage);

    // Atomic Operations
    out << "\n" << colors::blue() << "--- Atomic StrongId (Thread-Safety) ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, atomic_basic_operations);
    FATP_RUN_TEST_NS(runner, strongid, atomic_exchange);
    FATP_RUN_TEST_NS(runner, strongid, atomic_compare_exchange);
    FATP_RUN_TEST_NS(runner, strongid, atomic_concurrent_reads);
    FATP_RUN_TEST_NS(runner, strongid, atomic_concurrent_increments);

    // Type Traits
    out << "\n" << colors::blue() << "--- Type Traits ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, is_strong_id_trait);

    // Sentinel/Validity Patterns
    out << "\n" << colors::blue() << "--- Sentinel/Validity Patterns ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, invalid_sentinel);
    FATP_RUN_TEST_NS(runner, strongid, is_valid_check);
    FATP_RUN_TEST_NS(runner, strongid, min_max_methods);

    // New Check Policies
    out << "\n" << colors::blue() << "--- New Check Policies ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, non_zero_policy_sentinel_factories_bypass_validation);
    FATP_RUN_TEST_NS(runner, strongid, range_policy_sentinel_factories_bypass_validation);
    FATP_RUN_TEST_NS(runner, strongid, non_zero_policy_valid);
    FATP_RUN_TEST_NS(runner, strongid, non_zero_policy_invalid);
    FATP_RUN_TEST_NS(runner, strongid, strictly_positive_policy_valid);
    FATP_RUN_TEST_NS(runner, strongid, strictly_positive_policy_zero_invalid);
    FATP_RUN_TEST_NS(runner, strongid, strictly_positive_policy_negative_invalid);
    FATP_RUN_TEST_NS(runner, strongid, range_policy_valid);
    FATP_RUN_TEST_NS(runner, strongid, range_policy_below_min_invalid);
    FATP_RUN_TEST_NS(runner, strongid, range_policy_above_max_invalid);

    // StrongId-to-StrongId Bitwise
    out << "\n" << colors::blue() << "--- StrongId-to-StrongId Bitwise ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, strongid, bitwise_and_strongid);
    FATP_RUN_TEST_NS(runner, strongid, bitwise_or_strongid);
    FATP_RUN_TEST_NS(runner, strongid, bitwise_xor_strongid);

    // Single sanity benchmark for zero-overhead validation
    strongid::run_zero_overhead_sanity_benchmark();

    // Summary
    return 0 == runner.print_summary() ? true : false;
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_StrongId() ? 0 : 1;
}
#endif
