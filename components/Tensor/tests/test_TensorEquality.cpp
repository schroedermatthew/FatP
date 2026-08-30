/** @file test_TensorEquality.cpp @brief Owner/view equality, approximation, and hash tests. */

/*
FATP_META:
  meta_version: 1
  component: TensorEquality
  file_role: test
  path: components/Tensor/tests/test_TensorEquality.cpp
  layer: Testing
  namespace: fat_p::testing::tensorequality
  summary: "Logical owner/view equality, policy dispatch, approximation, and hash-law tests."
  api_stability: in_work
  related:
    headers:
      - include/fat_p/EqualityComparisons.h
      - include/fat_p/TensorAlgorithms.h
      - include/fat_p/TensorEquality.h
      - include/fat_p/Tensor.h
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

#include "EqualityComparisons.h"
#include "FatPTest.h"
#include "TensorAlgorithms.h"
#include "TensorEquality.h"

#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace fat_p::testing::tensorequality
{

FATP_TEST_CASE(exact_owner_values)
{
    const Tensor<int> equalLeft({2, 3}, 42);
    const Tensor<int> equalRight({2, 3}, 42);
    const Tensor<int> differentValue({2, 3}, 43);
    const Tensor<int> differentShape({3, 2}, 42);
    FATP_ASSERT_TRUE(equalLeft == equalRight, "Equal owners should compare equal");
    FATP_ASSERT_FALSE(equalLeft == differentValue, "Different values should compare unequal");
    FATP_ASSERT_FALSE(equalLeft == differentShape, "Different extents should compare unequal");
    return true;
}

FATP_TEST_CASE(readable_layout_independence)
{
    Tensor<int> contiguous({2, 3});
    for (std::size_t index = 0; index < contiguous.size(); ++index)
    {
        contiguous[index] = static_cast<int>(index + 1);
    }
    int physical[]{1, 4, 2, 5, 3, 6};
    const auto strided = TensorView<const int>::borrow(
        physical, TensorLayout(6, 0, DynamicExtents{2, 3}, TensorStrides{1, 2}));
    FATP_ASSERT_TRUE(exactEqual(contiguous, strided), "Readable equality should ignore physical layout");
    FATP_ASSERT_EQ(tensor_detail::hashKernel(contiguous, std::hash<int>{}),
                   tensor_detail::hashKernel(strided, std::hash<int>{}),
                   "Logically equal mappings should hash equally");
    return true;
}

FATP_TEST_CASE(approximate_values)
{
    const Tensor<double> exact({2}, 1.0);
    const Tensor<double> close({2}, 1.0 + 1e-8);
    const Tensor<double> scaled({2}, 1.0e8 + 1.0);
    const Tensor<double> scaleReference({2}, 1.0e8);
    FATP_ASSERT_TRUE(approxEqual(exact, close, 1e-7), "Absolute tolerance should accept close values");
    FATP_ASSERT_FALSE(approxEqual(exact, close, 1e-10), "Absolute tolerance should reject distant values");
    FATP_ASSERT_TRUE(approxEqual(scaleReference, scaled, 0.0, 1e-7),
                     "Relative tolerance should scale with magnitude");
    return true;
}

FATP_TEST_CASE(policy_dispatch_reports_mismatch)
{
    Tensor<double> expected({2}, 1.0);
    Tensor<double> close({2}, 1.0 + 1e-8);
    FATP_ASSERT_TRUE(areEqual(expected, close, 1e-7), "EqualDispatcher should honor comparison policy epsilon");
    close[1] = 9.0;
    FATP_ASSERT_FALSE(areEqual(expected, close, 1e-7),
                      "EqualDispatcher should return false after recording any mismatch");
    return true;
}

FATP_TEST_CASE(hash_equality_law)
{
    const Tensor<double> positiveZero({1}, 0.0);
    const Tensor<double> negativeZero({1}, -0.0);
    FATP_ASSERT_TRUE(positiveZero == negativeZero, "Signed zeros should compare equal");
    FATP_ASSERT_EQ(std::hash<Tensor<double>>{}(positiveZero), std::hash<Tensor<double>>{}(negativeZero),
                   "Signed-zero equality should be reflected by owner hashing");

    const Tensor<int> owner({2, 2}, 5);
    const Tensor<int> copy(owner);
    FATP_ASSERT_EQ(std::hash<Tensor<int>>{}(owner), std::hash<Tensor<int>>{}(copy),
                   "Deep copies with equal values should hash equally");
    return true;
}

FATP_TEST_CASE(unordered_owner_keys)
{
    std::unordered_map<Tensor<int>, int> map;
    map[Tensor<int>({2}, 4)] = 7;
    FATP_ASSERT_EQ(map.at(Tensor<int>({2}, 4)), 7, "Equivalent owner should find unordered-map key");

    std::unordered_set<Tensor<int>> set;
    set.insert(Tensor<int>({2}, 4));
    set.insert(Tensor<int>({2}, 4));
    set.insert(Tensor<int>({2}, 5));
    FATP_ASSERT_EQ(set.size(), std::size_t{2}, "Unordered set should merge equal owner values");
    return true;
}

FATP_TEST_CASE(scalar_empty_and_special_values)
{
    const Tensor<int> scalarLeft({}, 4);
    const Tensor<int> scalarRight({}, 4);
    FATP_ASSERT_TRUE(scalarLeft == scalarRight, "Rank-zero scalar equality should compare its value");

    const Tensor<int> emptyLeft({0, 3});
    const Tensor<int> emptyRight({0, 3});
    FATP_ASSERT_TRUE(emptyLeft == emptyRight, "Equal zero-extent owners should compare equal");

    const Tensor<double> positiveInfinity({1}, std::numeric_limits<double>::infinity());
    const Tensor<double> negativeInfinity({1}, -std::numeric_limits<double>::infinity());
    FATP_ASSERT_FALSE(positiveInfinity == negativeInfinity, "Opposite infinities should compare unequal");
    return true;
}

} // namespace fat_p::testing::tensorequality

namespace fat_p::testing
{

bool test_TensorEquality()
{
    FATP_PRINT_HEADER(TENSOR EQUALITY)
    TestRunner runner;
    FATP_RUN_TEST_NS(runner, tensorequality, exact_owner_values);
    FATP_RUN_TEST_NS(runner, tensorequality, readable_layout_independence);
    FATP_RUN_TEST_NS(runner, tensorequality, approximate_values);
    FATP_RUN_TEST_NS(runner, tensorequality, policy_dispatch_reports_mismatch);
    FATP_RUN_TEST_NS(runner, tensorequality, hash_equality_law);
    FATP_RUN_TEST_NS(runner, tensorequality, unordered_owner_keys);
    FATP_RUN_TEST_NS(runner, tensorequality, scalar_empty_and_special_values);
    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_TensorEquality() ? 0 : 1;
}
#endif
