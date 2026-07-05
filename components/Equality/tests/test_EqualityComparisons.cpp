/**
 * @file test_EqualityComparisons.cpp
 * @brief Comprehensive test suite for fat_p::EqualityComparisons
 *
 * Tests all features including:
 * - All comparison policies (Standard, ULP, Relative, Hybrid)
 * - Container comparisons (vector, map, set, array)
 * - Unordered container comparisons (unordered_set, unordered_map)
 * - Multi-container comparisons (unordered_multiset, unordered_multimap)
 * - Pair and tuple comparisons
 * - Nested container comparisons
 * - Edge cases (empty, single element, size mismatch)
 * - Floating-point edge cases (infinity, NaN, denormals, signed zeros)
 *
 * REGRESSION TESTS:
 * - Multiset multiplicity validation (consume-matching algorithm)
 * - Deep nesting support
 */
/*
FATP_META:
  meta_version: 1
  component: Equality
  file_role: test
  path: components/Equality/tests/test_EqualityComparisons.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for EqualityComparisons."
  api_stability: in_work
  related:
    docs_search: "EqualityComparisons"
    headers:
      - include/fat_p/EqualityComparisons.h
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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <deque>
#include <forward_list>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "EqualityComparisons.h"
#include "FatPTest.h"

namespace fat_p::testing::equalitycomparisons
{

// ============================================================================
// Test Constants
// ============================================================================

constexpr double kEps = std::numeric_limits<double>::epsilon();
constexpr float kEpsF = std::numeric_limits<float>::epsilon();

// ============================================================================
// Test Suite 1: StandardComparisonPolicy Edge Cases
// ============================================================================

FATP_TEST_CASE(standard_policy_infinities)
{
    constexpr double pos_inf = std::numeric_limits<double>::infinity();
    constexpr double neg_inf = -std::numeric_limits<double>::infinity();

    // Same sign infinities should be equal
    FATP_ASSERT_TRUE(areEqual(pos_inf, pos_inf), "Positive infinity equals itself");
    FATP_ASSERT_TRUE(areEqual(neg_inf, neg_inf), "Negative infinity equals itself");

    // Different sign infinities should NOT be equal
    FATP_ASSERT_FALSE(areEqual(pos_inf, neg_inf), "Opposite infinities not equal");

    // Infinity compared to finite should NOT be equal
    FATP_ASSERT_FALSE(areEqual(pos_inf, 1e308), "Infinity != finite");
    FATP_ASSERT_FALSE(areEqual(1e308, pos_inf), "Finite != infinity");

    return true;
}

FATP_TEST_CASE(standard_policy_nans)
{
    constexpr double nan1 = std::numeric_limits<double>::quiet_NaN();
    double nan2 = std::nan("");

    // NaN should never equal anything, even itself (IEEE 754)
    FATP_ASSERT_FALSE(areEqual(nan1, nan1), "NaN != NaN (IEEE 754)");
    FATP_ASSERT_FALSE(areEqual(nan1, nan2), "Different NaNs not equal");
    FATP_ASSERT_FALSE(areEqual(nan1, 0.0), "NaN != finite");

    return true;
}

FATP_TEST_CASE(standard_policy_signed_zeros)
{
    double pos_zero = 0.0;
    double neg_zero = -0.0;

    // IEEE 754: +0 == -0
    FATP_ASSERT_TRUE(areEqual(pos_zero, neg_zero), "+0 equals -0 (IEEE 754)");
    FATP_ASSERT_TRUE(areEqual(neg_zero, pos_zero), "-0 equals +0 (IEEE 754)");

    return true;
}

FATP_TEST_CASE(standard_policy_denormals)
{
    constexpr double denorm1 = std::numeric_limits<double>::denorm_min();
    double denorm2 = denorm1 + denorm1; // 2x denorm_min

    // Denormal should equal itself
    FATP_ASSERT_TRUE(areEqual(denorm1, denorm1), "Denormal equals itself");

    // Two close denormals should be equal under default tolerance
    FATP_ASSERT_TRUE(areEqual(denorm1, denorm2), "Close denormals are equal");

    return true;
}

// ============================================================================
// Test Suite 2: UlpComparisonPolicy
// ============================================================================

FATP_TEST_CASE(ulp_policy_basic)
{
    float a = 1.0f;
    float b = 1.0f + kEpsF;

    // Within 4 ULPs (default)
    FATP_ASSERT_TRUE((areEqual<float, UlpComparisonPolicy>(a, a)), "Same value");
    FATP_ASSERT_TRUE((areEqual<float, UlpComparisonPolicy>(a, b, 4.0f)), "Within 4 ULPs");

    // More than 4 ULPs apart
    float c = 1.0f + 10.0f * kEpsF;
    FATP_ASSERT_FALSE((areEqual<float, UlpComparisonPolicy>(a, c, 4.0f)), "Beyond 4 ULPs");

    return true;
}

FATP_TEST_CASE(ulp_policy_subnormals)
{
    // Float subnormals
    constexpr float denorm_f = std::numeric_limits<float>::denorm_min();
    FATP_ASSERT_TRUE((areEqual<float, UlpComparisonPolicy>(denorm_f, denorm_f)), "Float subnormal equals itself");

    // Double subnormals
    constexpr double denorm_d = std::numeric_limits<double>::denorm_min();
    FATP_ASSERT_TRUE((areEqual<double, UlpComparisonPolicy>(denorm_d, denorm_d)), "Double subnormal equals itself");

    return true;
}

FATP_TEST_CASE(ulp_policy_sign_matters)
{
    double a = 1.0;
    double b = -1.0;

    // Different signs should NOT be equal (even if same magnitude)
    FATP_ASSERT_FALSE((areEqual<double, UlpComparisonPolicy>(a, b)), "Opposite signs not equal");

    return true;
}

// ============================================================================
// Test Suite 3: RelativeComparisonPolicy
// ============================================================================

FATP_TEST_CASE(relative_policy_scale_independence)
{
    double relEps = 1e-5;

    // Small values
    double a1 = 1.0;
    double b1 = 1.0 + 0.5e-5;
    FATP_ASSERT_TRUE((areEqual<double, RelativeComparisonPolicy>(a1, b1, relEps)), "Small values within tolerance");

    // Large values (same relative error)
    double a2 = 1e6;
    double b2 = 1e6 + 0.5;
    FATP_ASSERT_TRUE((areEqual<double, RelativeComparisonPolicy>(a2, b2, relEps)), "Large values within tolerance");

    return true;
}

// ============================================================================
// Test Suite 4: HybridComparisonPolicy
// ============================================================================

FATP_TEST_CASE(hybrid_policy_robust)
{
    double relEps = 1e-5;
    double absEps = 1e-8;

    // Near zero - absolute tolerance should dominate
    double a1 = 1e-10;
    double b1 = 2e-10;
    FATP_ASSERT_TRUE((areEqual<double, HybridComparisonPolicy>(a1, b1, relEps, absEps)),
                     "Near zero handled by absolute tolerance");

    // Large values - relative tolerance should dominate
    double a2 = 1e6;
    double b2 = 1e6 + 0.5;
    FATP_ASSERT_TRUE((areEqual<double, HybridComparisonPolicy>(a2, b2, relEps, absEps)),
                     "Large values handled by relative tolerance");

    return true;
}

// ============================================================================
// Test Suite 5: Basic Container Comparisons
// ============================================================================

FATP_TEST_CASE(vector_basic)
{
    std::vector<double> v1 = {1.0, 2.0, 3.0};
    std::vector<double> v2 = {1.0, 2.0, 3.0};
    std::vector<double> v3 = {1.0, 2.0, 3.0 + 50.0 * kEps};
    std::vector<double> v4 = {1.0, 2.0};

    FATP_ASSERT_TRUE(areEqual(v1, v2), "Identical vectors");
    FATP_ASSERT_TRUE(areEqual(v1, v3), "Vectors within epsilon");
    FATP_ASSERT_FALSE(areEqual(v1, v4), "Different size vectors");

    return true;
}

FATP_TEST_CASE(vector_empty)
{
    std::vector<int> empty1;
    std::vector<int> empty2;
    std::vector<int> nonempty = {1};

    FATP_ASSERT_TRUE(areEqual(empty1, empty2), "Empty vectors are equal");
    FATP_ASSERT_FALSE(areEqual(empty1, nonempty), "Empty != non-empty");

    return true;
}

FATP_TEST_CASE(vector_bool_proxy)
{
    // REGRESSION: vector<bool> uses proxy references, not bool&.
    // This tests that the dispatcher handles proxy iterators correctly.
    std::vector<bool> vb1 = {true, false, true, false};
    std::vector<bool> vb2 = {true, false, true, false};
    std::vector<bool> vb3 = {true, false, false, false};
    std::vector<bool> vb4 = {true, false, true};

    FATP_ASSERT_TRUE(areEqual(vb1, vb2), "Identical vector<bool>");
    FATP_ASSERT_FALSE(areEqual(vb1, vb3), "Different element in vector<bool>");
    FATP_ASSERT_FALSE(areEqual(vb1, vb4), "Different size vector<bool>");

    // Nested: vector<vector<bool>>
    std::vector<std::vector<bool>> nested1 = {{true, false}, {false, true}};
    std::vector<std::vector<bool>> nested2 = {{true, false}, {false, true}};
    std::vector<std::vector<bool>> nested3 = {{true, false}, {true, true}};

    FATP_ASSERT_TRUE(areEqual(nested1, nested2), "Identical nested vector<bool>");
    FATP_ASSERT_FALSE(areEqual(nested1, nested3), "Different nested vector<bool>");

    return true;
}

FATP_TEST_CASE(array_comparison)
{
    std::array<double, 3> a1 = {1.0, 2.0, 3.0};
    std::array<double, 3> a2 = {1.0, 2.0, 3.0};
    std::array<double, 3> a3 = {1.0, 2.0, 4.0};

    FATP_ASSERT_TRUE(areEqual(a1, a2), "Identical arrays");
    FATP_ASSERT_FALSE(areEqual(a1, a3), "Different arrays");

    return true;
}

FATP_TEST_CASE(deque_comparison)
{
    std::deque<int> d1 = {1, 2, 3};
    std::deque<int> d2 = {1, 2, 3};
    std::deque<int> d3 = {1, 2, 4};

    FATP_ASSERT_TRUE(areEqual(d1, d2), "Identical deques");
    FATP_ASSERT_FALSE(areEqual(d1, d3), "Different deques");

    return true;
}

FATP_TEST_CASE(list_comparison)
{
    std::list<int> l1 = {1, 2, 3};
    std::list<int> l2 = {1, 2, 3};
    std::list<int> l3 = {1, 2, 4};

    FATP_ASSERT_TRUE(areEqual(l1, l2), "Identical lists");
    FATP_ASSERT_FALSE(areEqual(l1, l3), "Different lists");

    return true;
}

FATP_TEST_CASE(forward_list_comparison)
{
    // forward_list has no .size() method - tests non-sized iterable path
    std::forward_list<int> fl1 = {1, 2, 3};
    std::forward_list<int> fl2 = {1, 2, 3};
    std::forward_list<int> fl3 = {1, 2, 4};
    std::forward_list<int> fl4 = {1, 2, 3, 4}; // Different length

    FATP_ASSERT_TRUE(areEqual(fl1, fl2), "Identical forward_lists");
    FATP_ASSERT_FALSE(areEqual(fl1, fl3), "Different element in forward_list");
    FATP_ASSERT_FALSE(areEqual(fl1, fl4), "Different length forward_lists");

    return true;
}

FATP_TEST_CASE(forward_list_empty)
{
    std::forward_list<int> empty1;
    std::forward_list<int> empty2;
    std::forward_list<int> nonempty = {1};

    FATP_ASSERT_TRUE(areEqual(empty1, empty2), "Empty forward_lists are equal");
    FATP_ASSERT_FALSE(areEqual(empty1, nonempty), "Empty vs non-empty forward_list");

    return true;
}

FATP_TEST_CASE(string_view_comparison)
{
    using namespace std::string_view_literals;

    std::string_view sv1 = "hello"sv;
    std::string_view sv2 = "hello"sv;
    std::string_view sv3 = "world"sv;
    std::string_view sv4 = ""sv;

    FATP_ASSERT_TRUE(areEqual(sv1, sv2), "Identical string_views");
    FATP_ASSERT_FALSE(areEqual(sv1, sv3), "Different string_views");
    FATP_ASSERT_FALSE(areEqual(sv1, sv4), "Non-empty vs empty string_view");
    FATP_ASSERT_TRUE(areEqual(sv4, ""sv), "Empty string_views equal");

    return true;
}

// ============================================================================
// Test Suite 6: Ordered Associative Containers
// ============================================================================

FATP_TEST_CASE(map_basic)
{
    std::map<std::string, double> m1 = {{"a", 1.0}, {"b", 2.0}};
    std::map<std::string, double> m2 = {{"a", 1.0}, {"b", 2.0}};
    std::map<std::string, double> m3 = {{"a", 1.0}, {"b", 2.0 + 50.0 * kEps}};
    std::map<std::string, double> m4 = {{"a", 1.0}};

    FATP_ASSERT_TRUE(areEqual(m1, m2), "Identical maps");
    FATP_ASSERT_TRUE(areEqual(m1, m3), "Maps within epsilon");
    FATP_ASSERT_FALSE(areEqual(m1, m4), "Different size maps");

    return true;
}

FATP_TEST_CASE(map_empty)
{
    std::map<int, int> empty1;
    std::map<int, int> empty2;
    std::map<int, int> nonempty = {{1, 1}};

    FATP_ASSERT_TRUE(areEqual(empty1, empty2), "Empty maps are equal");
    FATP_ASSERT_FALSE(areEqual(empty1, nonempty), "Empty != non-empty");

    return true;
}

FATP_TEST_CASE(map_string_keys)
{
    // REGRESSION: map keys are const K, so string keys must use decay_t
    // to hit the string fast path instead of character-by-character iteration
    std::map<std::string, double> m1 = {{"hello", 1.0}, {"world", 2.0}};
    std::map<std::string, double> m2 = {{"hello", 1.0 + 50.0 * kEps}, {"world", 2.0}};
    std::map<std::string, double> m3 = {{"hello", 1.0}, {"other", 2.0}};
    std::map<std::string, double> m4 = {{"hello", 9.0}, {"world", 2.0}};

    FATP_ASSERT_TRUE(areEqual(m1, m2), "Map with string keys within epsilon");
    FATP_ASSERT_FALSE(areEqual(m1, m3), "Map with different string keys");
    FATP_ASSERT_FALSE(areEqual(m1, m4), "Map with different values");

    return true;
}

FATP_TEST_CASE(set_basic)
{
    std::set<int> s1 = {1, 2, 3};
    std::set<int> s2 = {1, 2, 3};
    std::set<int> s3 = {1, 2, 4};
    std::set<int> s4 = {1, 2};

    FATP_ASSERT_TRUE(areEqual(s1, s2), "Identical sets");
    FATP_ASSERT_FALSE(areEqual(s1, s3), "Different element sets");
    FATP_ASSERT_FALSE(areEqual(s1, s4), "Different size sets");

    return true;
}

FATP_TEST_CASE(multiset_basic)
{
    std::multiset<int> ms1 = {1, 1, 2, 3};
    std::multiset<int> ms2 = {1, 1, 2, 3};
    std::multiset<int> ms3 = {1, 2, 2, 3}; // Same elements, different multiplicities

    FATP_ASSERT_TRUE(areEqual(ms1, ms2), "Identical multisets");
    FATP_ASSERT_FALSE(areEqual(ms1, ms3), "Different multiplicities");

    return true;
}

FATP_TEST_CASE(multimap_basic)
{
    std::multimap<int, std::string> mm1 = {{1, "a"}, {1, "b"}, {2, "c"}};
    std::multimap<int, std::string> mm2 = {{1, "a"}, {1, "b"}, {2, "c"}};
    std::multimap<int, std::string> mm3 = {{1, "a"}, {1, "x"}, {2, "c"}};

    FATP_ASSERT_TRUE(areEqual(mm1, mm2), "Identical multimaps");
    FATP_ASSERT_FALSE(areEqual(mm1, mm3), "Different values in multimap");

    return true;
}

// ============================================================================
// Test Suite 7: Unordered Containers
// ============================================================================

FATP_TEST_CASE(unordered_set_basic)
{
    std::unordered_set<int> us1 = {1, 2, 3};
    std::unordered_set<int> us2 = {3, 2, 1}; // Same elements, different insertion order
    std::unordered_set<int> us3 = {1, 2, 4};
    std::unordered_set<int> us4 = {1, 2};

    FATP_ASSERT_TRUE(areEqual(us1, us2), "Unordered sets with same elements");
    FATP_ASSERT_FALSE(areEqual(us1, us3), "Different element unordered sets");
    FATP_ASSERT_FALSE(areEqual(us1, us4), "Different size unordered sets");

    return true;
}

FATP_TEST_CASE(unordered_set_empty)
{
    std::unordered_set<int> empty1;
    std::unordered_set<int> empty2;
    std::unordered_set<int> nonempty = {1};

    FATP_ASSERT_TRUE(areEqual(empty1, empty2), "Empty unordered sets are equal");
    FATP_ASSERT_FALSE(areEqual(empty1, nonempty), "Empty != non-empty unordered set");

    return true;
}

FATP_TEST_CASE(unordered_map_basic)
{
    std::unordered_map<std::string, double> um1 = {{"a", 1.0}, {"b", 2.0}};
    std::unordered_map<std::string, double> um2 = {{"b", 2.0}, {"a", 1.0}};
    std::unordered_map<std::string, double> um3 = {{"a", 1.0}, {"b", 2.0 + 50.0 * kEps}};
    std::unordered_map<std::string, double> um4 = {{"a", 1.0}, {"c", 2.0}};

    FATP_ASSERT_TRUE(areEqual(um1, um2), "Unordered maps with same entries");
    FATP_ASSERT_TRUE(areEqual(um1, um3), "Unordered maps within epsilon");
    FATP_ASSERT_FALSE(areEqual(um1, um4), "Different key unordered maps");

    return true;
}

FATP_TEST_CASE(unordered_map_empty)
{
    std::unordered_map<int, int> empty1;
    std::unordered_map<int, int> empty2;
    std::unordered_map<int, int> nonempty = {{1, 1}};

    FATP_ASSERT_TRUE(areEqual(empty1, empty2), "Empty unordered maps are equal");
    FATP_ASSERT_FALSE(areEqual(empty1, nonempty), "Empty != non-empty unordered map");

    return true;
}

// ============================================================================
// Test Suite 8: Unordered Multi-Containers (REGRESSION TESTS)
// ============================================================================
// These tests verify the multiplicity validation that was falsely claimed
// was broken. The {1,1,1} vs {1,2,3} case MUST return false.

FATP_TEST_CASE(unordered_multiset_multiplicity_regression)
{
    // REGRESSION: Multisets with same elements but different multiplicities must differ
    std::unordered_multiset<int> ms1 = {1, 1, 1};
    std::unordered_multiset<int> ms2 = {1, 2, 3};

    FATP_ASSERT_FALSE(areEqual(ms1, ms2), "REGRESSION: {1,1,1} vs {1,2,3} must be NOT EQUAL");

    return true;
}

FATP_TEST_CASE(unordered_multiset_basic)
{
    std::unordered_multiset<int> ums1 = {1, 1, 2, 3};
    std::unordered_multiset<int> ums2 = {3, 1, 2, 1}; // Same with different order
    std::unordered_multiset<int> ums3 = {1, 2, 2, 3}; // Different multiplicities

    FATP_ASSERT_TRUE(areEqual(ums1, ums2), "Same multiset different order");
    FATP_ASSERT_FALSE(areEqual(ums1, ums3), "Different multiplicities");

    return true;
}

FATP_TEST_CASE(unordered_multiset_empty)
{
    std::unordered_multiset<int> empty1;
    std::unordered_multiset<int> empty2;

    FATP_ASSERT_TRUE(areEqual(empty1, empty2), "Empty unordered multisets are equal");

    return true;
}

FATP_TEST_CASE(unordered_multimap_basic)
{
    std::unordered_multimap<int, std::string> umm1 = {{1, "a"}, {1, "b"}, {2, "c"}};
    std::unordered_multimap<int, std::string> umm2 = {{2, "c"}, {1, "b"}, {1, "a"}};
    std::unordered_multimap<int, std::string> umm3 = {{1, "a"}, {1, "x"}, {2, "c"}};

    FATP_ASSERT_TRUE(areEqual(umm1, umm2), "Same multimap different order");
    FATP_ASSERT_FALSE(areEqual(umm1, umm3), "Different values in multimap");

    return true;
}

FATP_TEST_CASE(unordered_multimap_with_epsilon)
{
    std::unordered_multimap<int, double> umm1 = {{1, 1.0}, {1, 2.0}, {2, 3.0}};
    std::unordered_multimap<int, double> umm2 = {{1, 1.0 + 50.0 * kEps}, {1, 2.0}, {2, 3.0}};
    std::unordered_multimap<int, double> umm3 = {{1, 1.0}, {1, 9.0}, {2, 3.0}};

    FATP_ASSERT_TRUE(areEqual(umm1, umm2), "Multimap within epsilon");
    FATP_ASSERT_FALSE(areEqual(umm1, umm3), "Multimap value mismatch");

    return true;
}

FATP_TEST_CASE(unordered_multimap_multiplicity)
{
    std::unordered_multimap<int, int> umm1 = {{1, 1}, {1, 1}, {1, 1}};
    std::unordered_multimap<int, int> umm2 = {{1, 1}, {1, 1}};

    FATP_ASSERT_FALSE(areEqual(umm1, umm2), "Different multiplicities for same key");

    return true;
}

FATP_TEST_CASE(unordered_multimap_floating_multiplicity)
{
    // Stress test: duplicated keys with floating values near tolerance
    // Tests interaction between epsilon matching and multiplicity counting
    constexpr double tiny = 50.0 * kEps;

    std::unordered_multimap<int, double> umm1 = {{1, 1.0},
                                                 {1, 1.0 + tiny},
                                                 {1, 2.0}, // key=1 has 3 values
                                                 {2, 3.0}};
    std::unordered_multimap<int, double> umm2 = {{1, 1.0 + tiny / 2},
                                                 {1, 1.0 + tiny},
                                                 {1, 2.0}, // same structure, within epsilon
                                                 {2, 3.0}};
    std::unordered_multimap<int, double> umm3 = {{1, 1.0},
                                                 {1, 1.0 + tiny}, // only 2 values for key=1 (multiplicity mismatch)
                                                 {2, 3.0}};

    FATP_ASSERT_TRUE(areEqual(umm1, umm2), "Floating multimap within epsilon");
    FATP_ASSERT_FALSE(areEqual(umm1, umm3), "Floating multimap multiplicity mismatch");

    return true;
}

FATP_TEST_CASE(unordered_multimap_epsilon_pairing)
{
    // KNOWN LIMITATION: Greedy first-match can produce false negatives
    // when epsilon equality is non-transitive.
    //
    // Example: A = {1.0, 2.0}, B = {1.5, 2.0} with eps = 0.6
    //   - 1.0 matches 1.5 (diff = 0.5 < 0.6) [OK]
    //   - 1.0 matches 2.0 (diff = 1.0 > 0.6) [X]
    //   - 2.0 matches 1.5 (diff = 0.5 < 0.6) [OK]
    //   - 2.0 matches 2.0 (diff = 0.0 < 0.6) [OK]
    //
    // Perfect matching exists: (1.0<->1.5, 2.0<->2.0)
    // But greedy may pick: 1.0<->2.0? No. 1.0<->1.5, then 2.0 has no match? No, 2.0<->2.0 works.
    // Actually this case works. Let's construct a real adversarial case:
    //
    // A = {1.0, 1.4}, B = {1.5, 1.1} with eps such that:
    //   - 1.0 matches 1.1 (diff=0.1) and 1.5 (diff=0.5 if eps>0.5)
    //   - 1.4 matches 1.5 (diff=0.1) and 1.1 (diff=0.3)
    // With eps=0.2: 1.0<->1.1 only, 1.4<->1.5 only -- unique, greedy works
    // With eps=0.5: 1.0<->{1.1,1.5}, 1.4<->{1.5,1.1} -- ambiguous
    //   Greedy iteration order dependent. If we check 1.0 first and it grabs 1.5,
    //   then 1.4 can only match 1.1 (diff=0.3<0.5) -- works!
    //   But if 1.0 grabs 1.1, then 1.4 grabs 1.5 -- also works!
    //
    // This is actually hard to break with our algorithm because equal_range
    // groups by key, and within a key bucket we do first-available matching.
    // The failure case would need values that "steal" matches from siblings.
    //
    // For now, document that greedy matching is used and may produce
    // false negatives in pathological epsilon cases.

    std::unordered_multimap<int, double> umm1 = {{1, 1.0}, {1, 1.4}};
    std::unordered_multimap<int, double> umm2 = {{1, 1.1}, {1, 1.5}};

    // With eps=0.2, unique matching exists: 1.0<->1.1, 1.4<->1.5
    bool result = areEqual(umm1, umm2, 0.2);
    FATP_ASSERT_TRUE(result, "Greedy match should find valid pairing with tight epsilon");

    return true;
}

FATP_TEST_CASE(vector_containing_nan)
{
    // NaN inside containers: ensure container logic doesn't accidentally
    // treat NaNs as equal via "diff <= eps" shortcut
    const double nan = std::numeric_limits<double>::quiet_NaN();

    std::vector<double> v1 = {1.0, nan, 3.0};
    std::vector<double> v2 = {1.0, nan, 3.0};
    std::vector<double> v3 = {1.0, 2.0, 3.0};

    // NaN != NaN, even inside containers
    FATP_ASSERT_FALSE(areEqual(v1, v2), "Vectors with NaN should not be equal (NaN != NaN)");
    FATP_ASSERT_FALSE(areEqual(v1, v3), "Vector with NaN vs without should differ");

    return true;
}

FATP_TEST_CASE(symmetry_check)
{
    // Meta-test: areEqual(a,b) == areEqual(b,a) for various types
    std::vector<double> v1 = {1.0, 2.0, 3.0};
    std::vector<double> v2 = {1.0, 2.0, 3.1};

    FATP_ASSERT_EQ(areEqual(v1, v2), areEqual(v2, v1), "Vector comparison should be symmetric");

    std::map<int, double> m1 = {{1, 1.0}, {2, 2.0}};
    std::map<int, double> m2 = {{1, 1.0}, {2, 2.1}};

    FATP_ASSERT_EQ(areEqual(m1, m2), areEqual(m2, m1), "Map comparison should be symmetric");

    std::unordered_multiset<int> ums1 = {1, 1, 2};
    std::unordered_multiset<int> ums2 = {1, 2, 2};

    FATP_ASSERT_EQ(areEqual(ums1, ums2), areEqual(ums2, ums1), "Multiset comparison should be symmetric");

    return true;
}

// ============================================================================
// Test Suite 9: Pair and Tuple Comparisons
// ============================================================================

FATP_TEST_CASE(pair_basic)
{
    std::pair<double, double> p1 = {1.0, 2.0};
    std::pair<double, double> p2 = {1.0, 2.0};
    std::pair<double, double> p3 = {1.0 + 50.0 * kEps, 2.0};
    std::pair<double, double> p4 = {1.0, 9.0};

    FATP_ASSERT_TRUE(areEqual(p1, p2), "Identical pairs");
    FATP_ASSERT_TRUE(areEqual(p1, p3), "Pairs within epsilon");
    FATP_ASSERT_FALSE(areEqual(p1, p4), "Different pairs");

    return true;
}

FATP_TEST_CASE(pair_mixed_types)
{
    std::pair<std::string, int> p1 = {"hello", 42};
    std::pair<std::string, int> p2 = {"hello", 42};
    std::pair<std::string, int> p3 = {"world", 42};
    std::pair<std::string, int> p4 = {"hello", 99};

    FATP_ASSERT_TRUE(areEqual(p1, p2), "Identical string-int pairs");
    FATP_ASSERT_FALSE(areEqual(p1, p3), "Different first element");
    FATP_ASSERT_FALSE(areEqual(p1, p4), "Different second element");

    return true;
}

FATP_TEST_CASE(tuple_basic)
{
    std::tuple<double, int, std::string> t1 = {1.0, 42, "test"};
    std::tuple<double, int, std::string> t2 = {1.0, 42, "test"};
    std::tuple<double, int, std::string> t3 = {1.0 + 50.0 * kEps, 42, "test"};
    std::tuple<double, int, std::string> t4 = {1.0, 42, "fail"};

    FATP_ASSERT_TRUE(areEqual(t1, t2), "Identical tuples");
    FATP_ASSERT_TRUE(areEqual(t1, t3), "Tuples within epsilon");
    FATP_ASSERT_FALSE(areEqual(t1, t4), "Different string in tuple");

    return true;
}

FATP_TEST_CASE(tuple_empty)
{
    std::tuple<> t1;
    std::tuple<> t2;

    FATP_ASSERT_TRUE(areEqual(t1, t2), "Empty tuples are equal");

    return true;
}

FATP_TEST_CASE(tuple_single_element)
{
    std::tuple<int> t1 = {42};
    std::tuple<int> t2 = {42};
    std::tuple<int> t3 = {99};

    FATP_ASSERT_TRUE(areEqual(t1, t2), "Single element tuples equal");
    FATP_ASSERT_FALSE(areEqual(t1, t3), "Single element tuples differ");

    return true;
}

// ============================================================================
// Test Suite 10: Nested Container Comparisons
// ============================================================================

FATP_TEST_CASE(nested_vector_vector)
{
    std::vector<std::vector<double>> n1 = {{1.0, 2.0}, {3.0, 4.0}};
    std::vector<std::vector<double>> n2 = {{1.0, 2.0}, {3.0, 4.0}};
    std::vector<std::vector<double>> n3 = {{1.0, 2.0}, {3.0, 4.0 + 50.0 * kEps}};
    std::vector<std::vector<double>> n4 = {{1.0, 2.0}, {3.0, 9.0}};

    FATP_ASSERT_TRUE(areEqual(n1, n2), "Identical nested vectors");
    FATP_ASSERT_TRUE(areEqual(n1, n3), "Nested vectors within epsilon");
    FATP_ASSERT_FALSE(areEqual(n1, n4), "Different nested vectors");

    return true;
}

FATP_TEST_CASE(nested_map_vector)
{
    std::map<std::string, std::vector<double>> m1 = {{"a", {1.0, 2.0}}, {"b", {3.0, 4.0}}};
    std::map<std::string, std::vector<double>> m2 = {{"a", {1.0, 2.0}}, {"b", {3.0, 4.0}}};
    std::map<std::string, std::vector<double>> m3 = {{"a", {1.0, 2.0}}, {"b", {3.0, 9.0}}};

    FATP_ASSERT_TRUE(areEqual(m1, m2), "Identical map of vectors");
    FATP_ASSERT_FALSE(areEqual(m1, m3), "Different map of vectors");

    return true;
}

FATP_TEST_CASE(nested_vector_pair)
{
    std::vector<std::pair<int, double>> v1 = {{1, 1.0}, {2, 2.0}};
    std::vector<std::pair<int, double>> v2 = {{1, 1.0}, {2, 2.0}};
    std::vector<std::pair<int, double>> v3 = {{1, 1.0}, {2, 9.0}};

    FATP_ASSERT_TRUE(areEqual(v1, v2), "Identical vector of pairs");
    FATP_ASSERT_FALSE(areEqual(v1, v3), "Different vector of pairs");

    return true;
}

FATP_TEST_CASE(deep_nesting)
{
    // 4 levels deep: vector<map<string, vector<pair<int, double>>>>
    using Inner = std::pair<int, double>;
    using Level2 = std::vector<Inner>;
    using Level3 = std::map<std::string, Level2>;
    using Level4 = std::vector<Level3>;

    Level4 d1 = {{{{"a", {{1, 1.0}, {2, 2.0}}}}}, {{{"b", {{3, 3.0}}}}}};
    Level4 d2 = {{{{"a", {{1, 1.0}, {2, 2.0}}}}}, {{{"b", {{3, 3.0}}}}}};
    Level4 d3 = {
        {{{"a", {{1, 1.0}, {2, 2.0}}}}},
        {{{"b", {{3, 9.0}}}}} // Different value deep inside
    };

    FATP_ASSERT_TRUE(areEqual(d1, d2), "Identical deep nesting");
    FATP_ASSERT_FALSE(areEqual(d1, d3), "Different value in deep nesting");

    return true;
}

// ============================================================================
// Test Suite 11: Integer and Non-Floating Comparisons
// ============================================================================

FATP_TEST_CASE(integer_comparison)
{
    int a = 42;
    int b = 42;
    int c = 43;

    FATP_ASSERT_TRUE(areEqual(a, b), "Equal integers");
    FATP_ASSERT_FALSE(areEqual(a, c), "Unequal integers");

    return true;
}

FATP_TEST_CASE(string_comparison)
{
    std::string s1 = "hello";
    std::string s2 = "hello";
    std::string s3 = "world";
    std::string s4 = "";

    FATP_ASSERT_TRUE(areEqual(s1, s2), "Equal strings");
    FATP_ASSERT_FALSE(areEqual(s1, s3), "Unequal strings");
    FATP_ASSERT_FALSE(areEqual(s1, s4), "Non-empty != empty string");
    FATP_ASSERT_TRUE(areEqual(s4, std::string("")), "Empty strings equal");

    return true;
}

FATP_TEST_CASE(bool_comparison)
{
    FATP_ASSERT_TRUE(areEqual(true, true), "true == true");
    FATP_ASSERT_TRUE(areEqual(false, false), "false == false");
    FATP_ASSERT_FALSE(areEqual(true, false), "true != false");

    return true;
}

// ============================================================================
// Test Suite 12: Custom Epsilon Values
// ============================================================================

FATP_TEST_CASE(default_tolerance_contract)
{
    // CONTRACT: Default epsilon is 100 * std::numeric_limits<double>::epsilon()
    // This is approximately 2.22e-14 for IEEE 754 double precision.
    // Values within this tolerance are considered equal; values outside are not.

    constexpr double defaultEps = 100.0 * std::numeric_limits<double>::epsilon();

    // Just inside default tolerance (should be equal)
    double a1 = 1.0;
    double b1 = 1.0 + defaultEps * 0.5; // 50% of tolerance
    FATP_ASSERT_TRUE(areEqual(a1, b1), "Values within 50% of default tolerance should be equal");

    // At the boundary (should be equal - tolerance is inclusive)
    double a2 = 1.0;
    double b2 = 1.0 + defaultEps * 0.99; // 99% of tolerance
    FATP_ASSERT_TRUE(areEqual(a2, b2), "Values at 99% of default tolerance should be equal");

    // Just outside default tolerance (should NOT be equal)
    double a3 = 1.0;
    double b3 = 1.0 + defaultEps * 10.0; // 10x tolerance
    FATP_ASSERT_FALSE(areEqual(a3, b3), "Values at 10x default tolerance should NOT be equal");

    // Verify the actual default value matches our expectation
    // (This would catch if someone changes kDefaultDoubleEpsilon)
    constexpr double expectedDefault = std::numeric_limits<double>::epsilon() * 100.0;
    FATP_ASSERT_TRUE(areEqual(1.0, 1.0 + expectedDefault * 0.5), "Default tolerance should be ~100 * machine epsilon");

    return true;
}

FATP_TEST_CASE(custom_epsilon_scalar)
{
    double a = 1.0;
    double b = 1.1;

    FATP_ASSERT_FALSE(areEqual(a, b), "1.0 vs 1.1 with default epsilon");
    FATP_ASSERT_TRUE(areEqual(a, b, 0.2), "1.0 vs 1.1 with eps=0.2");
    FATP_ASSERT_FALSE(areEqual(a, b, 0.05), "1.0 vs 1.1 with eps=0.05");

    return true;
}

FATP_TEST_CASE(custom_epsilon_container)
{
    std::vector<double> v1 = {1.0, 2.0, 3.0};
    std::vector<double> v2 = {1.1, 2.1, 3.1};

    FATP_ASSERT_FALSE(areEqual(v1, v2), "Vectors differ with default epsilon");
    FATP_ASSERT_TRUE(areEqual(v1, v2, 0.2), "Vectors equal with eps=0.2");

    return true;
}

FATP_TEST_CASE(custom_epsilon_nested)
{
    std::map<std::string, std::vector<double>> m1 = {{"a", {1.0, 2.0}}};
    std::map<std::string, std::vector<double>> m2 = {{"a", {1.1, 2.1}}};

    FATP_ASSERT_FALSE(areEqual(m1, m2), "Nested differs with default epsilon");
    FATP_ASSERT_TRUE(areEqual(m1, m2, 0.2), "Nested equal with eps=0.2");

    return true;
}

// ============================================================================
// Test Suite 13: Convenience Functions
// ============================================================================

FATP_TEST_CASE(approximate_equal_convenience)
{
    double a = 1.0;
    double b = 1.0 + 50.0 * kEps;

    // Uses HybridComparisonPolicy by default
    bool result = approximateEqual(a, b);
    FATP_ASSERT_TRUE(result, "approximateEqual uses robust hybrid policy");

    return true;
}

FATP_TEST_CASE(float_equal_basic)
{
    double a = 1.0;
    double b = 1.0 + 50.0 * kEps;

    FATP_ASSERT_TRUE(floatEqual(a, b), "floatEqual with defaults");
    FATP_ASSERT_TRUE(floatEqual(1.0, 1.1, 0.2), "floatEqual with custom epsilon");
    FATP_ASSERT_FALSE(floatEqual(1.0, 1.1, 0.05), "floatEqual outside epsilon");

    return true;
}

FATP_TEST_CASE(float_equal_with_policies)
{
    double a = 1.0;
    double b = 1.0 + kEps;

    FATP_ASSERT_TRUE((floatEqual<double, StandardComparisonPolicy>(a, b)), "floatEqual with Standard policy");
    FATP_ASSERT_TRUE((floatEqual<double, UlpComparisonPolicy>(a, b, 4.0)), "floatEqual with ULP policy");
    FATP_ASSERT_TRUE((floatEqual<double, RelativeComparisonPolicy>(a, b, 1e-9)), "floatEqual with Relative policy");
    FATP_ASSERT_TRUE((floatEqual<double, HybridComparisonPolicy>(a, b, 1e-9, 1e-9)), "floatEqual with Hybrid policy");

    return true;
}

// ============================================================================
// Test Suite 14: Stress Testing
// ============================================================================

FATP_TEST_CASE(stress_large_vector)
{
    constexpr size_t N = 10000;
    std::vector<double> v1(N);
    std::vector<double> v2(N);

    for (size_t i = 0; i < N; ++i)
    {
        v1[i] = static_cast<double>(i);
        v2[i] = static_cast<double>(i);
    }

    FATP_ASSERT_TRUE(areEqual(v1, v2), "Large identical vectors");

    v2[N / 2] = 999999.0;
    FATP_ASSERT_FALSE(areEqual(v1, v2), "Large vectors with one difference");

    return true;
}

FATP_TEST_CASE(stress_random_operations)
{
    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::uniform_real_distribution<double> dist(-1000.0, 1000.0);

    for (int trial = 0; trial < 100; ++trial)
    {
        std::vector<double> v1, v2;
        size_t size = 10 + (rng() % 100);

        for (size_t i = 0; i < size; ++i)
        {
            double val = dist(rng);
            v1.push_back(val);
            v2.push_back(val);
        }

        FATP_ASSERT_TRUE(areEqual(v1, v2), "Random identical vectors");

        // Modify one element
        v2[rng() % size] += 1000.0;
        FATP_ASSERT_FALSE(areEqual(v1, v2), "Random modified vectors");
    }

    return true;
}

// ============================================================================
// Benchmarks (Sanity Check Only)
// ============================================================================
//
// EXPECTED RESULTS:
// - Fat-P areEqual and manual epsilon loop should be within ~2x of each other.
//   Fat-P has generic dispatch overhead; manual loop may auto-vectorize better.
// - std::equal (exact) should be ~5-10x faster than epsilon comparisons.
//   This is expected: epsilon math (fabs, fmax, comparisons) is inherently
//   more expensive than bitwise ==. This is NOT an apples-to-apples comparison.
//
// These benchmarks detect gross performance regressions, not micro-optimizations.
// Actual performance depends on compiler, CPU, and optimization level.

inline void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}


} // namespace fat_p::testing::equalitycomparisons

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing
{

bool test_EqualityComparisons()
{
    FATP_PRINT_HEADER(EQUALITY COMPARISONS)

    TestRunner runner;
    auto& out = *get_test_config().output;

    // Floating-point policies
    out << colors::blue() << "--- Floating-Point Policies ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalitycomparisons, standard_policy_infinities);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, standard_policy_nans);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, standard_policy_signed_zeros);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, standard_policy_denormals);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, ulp_policy_basic);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, ulp_policy_subnormals);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, ulp_policy_sign_matters);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, relative_policy_scale_independence);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, hybrid_policy_robust);

    // Basic containers
    out << "\n" << colors::blue() << "--- Basic Containers ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalitycomparisons, vector_basic);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, vector_empty);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, vector_bool_proxy);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, array_comparison);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, deque_comparison);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, list_comparison);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, forward_list_comparison);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, forward_list_empty);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, string_view_comparison);

    // Ordered associative containers
    out << "\n" << colors::blue() << "--- Ordered Associative Containers ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalitycomparisons, map_basic);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, map_empty);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, map_string_keys);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, set_basic);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, multiset_basic);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, multimap_basic);

    // Unordered containers
    out << "\n" << colors::blue() << "--- Unordered Containers ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalitycomparisons, unordered_set_basic);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, unordered_set_empty);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, unordered_map_basic);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, unordered_map_empty);

    // Unordered multi-containers (REGRESSION TESTS)
    out << "\n" << colors::blue() << "--- Unordered Multi-Containers (Regression) ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalitycomparisons, unordered_multiset_multiplicity_regression);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, unordered_multiset_basic);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, unordered_multiset_empty);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, unordered_multimap_basic);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, unordered_multimap_with_epsilon);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, unordered_multimap_multiplicity);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, unordered_multimap_floating_multiplicity);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, unordered_multimap_epsilon_pairing);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, vector_containing_nan);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, symmetry_check);

    // Pairs and tuples
    out << "\n" << colors::blue() << "--- Pairs and Tuples ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalitycomparisons, pair_basic);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, pair_mixed_types);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, tuple_basic);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, tuple_empty);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, tuple_single_element);

    // Nested containers
    out << "\n" << colors::blue() << "--- Nested Containers ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalitycomparisons, nested_vector_vector);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, nested_map_vector);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, nested_vector_pair);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, deep_nesting);

    // Scalar types
    out << "\n" << colors::blue() << "--- Scalar Types ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalitycomparisons, integer_comparison);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, string_comparison);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, bool_comparison);

    // Custom epsilon
    out << "\n" << colors::blue() << "--- Custom Epsilon ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalitycomparisons, default_tolerance_contract);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, custom_epsilon_scalar);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, custom_epsilon_container);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, custom_epsilon_nested);

    // Convenience functions
    out << "\n" << colors::blue() << "--- Convenience Functions ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalitycomparisons, approximate_equal_convenience);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, float_equal_basic);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, float_equal_with_policies);

    // Stress tests
    out << "\n" << colors::blue() << "--- Stress Tests ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, equalitycomparisons, stress_large_vector);
    FATP_RUN_TEST_NS(runner, equalitycomparisons, stress_random_operations);

    equalitycomparisons::run_benchmarks();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_EqualityComparisons() ? 0 : 1;
}
#endif
