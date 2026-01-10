/**
 * @file test_PolicyIterator.cpp
 * @brief Comprehensive unit tests for PolicyIterator.h and TensorStridePolicy.h
 *
 * Build commands:
 *   Standard:   g++ -std=c++17 -DENABLE_TEST_APPLICATION -I. -o test test_PolicyIterator.cpp
 *   Sanitizers: g++ -std=c++17 -DENABLE_TEST_APPLICATION -fsanitize=address,undefined -g -I. -o test test_PolicyIterator.cpp
 */
/*
FATP_META:
  meta_version: 1
  component: PolicyIterator
  file_role: test
  path: tests/test_PolicyIterator.cpp
  namespace: fat_p
  summary: "Unit tests for PolicyIterator."
  related:
    docs_search: "PolicyIterator"
    headers:
      - fat_p/PolicyIterator.h
      - fat_p/TensorStridePolicy.h
      - fat_p/TensorIteration.h
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

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "PolicyIterator.h"
#include "TensorStridePolicy.h"
#include "TensorIteration.h"
#include "FatPTest.h"

namespace fat_p::testing::policyiterator {

using namespace fat_p::iterator;

// ============================================================================
// Helper Types
// ============================================================================

struct Point { int x; int y; };

template <typename T>
bool check_sequence(const std::vector<T>& actual, const std::vector<T>& expected, const char* msg) {
    ASSERT_EQ(actual.size(), expected.size(), msg);
    for (size_t i = 0; i < actual.size(); ++i) {
        ASSERT_EQ(actual[i], expected[i], (std::string(msg) + " at index " + std::to_string(i)).c_str());
    }
    return true;
}

// Simple LCG for reproducible "randomness" in fuzz tests
class SimpleRng {
    uint32_t mState;
public:
    explicit SimpleRng(uint32_t seed = 12345) : mState(seed) {}
    uint32_t next() {
        mState = mState * 1103515245 + 12345;
        return (mState >> 16) & 0x7FFF;
    }
    int nextInt(int min, int max) {
        return min + static_cast<int>(next() % static_cast<uint32_t>(max - min + 1));
    }
};

// ============================================================================
// 1. Standard Policy Tests
// ============================================================================

TEST_CASE(standard_policy_basic) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    auto begin = PolicyIterator<int>::begin(data.data(), data.data() + data.size());
    auto end = PolicyIterator<int>::end(data.data(), data.data() + data.size());
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {1, 2, 3, 4, 5};
    return check_sequence(result, expected, "Standard policy iteration");
}

TEST_CASE(standard_policy_bidirectional) {
    std::vector<int> data = {10, 20, 30, 40, 50};
    
    auto it = PolicyIterator<int>::begin(data.data(), data.data() + data.size());
    
    // Forward to last element
    ++it; // at 20
    ++it; // at 30
    ++it; // at 40
    ++it; // at 50
    
    std::vector<int> result;
    result.push_back(*it); // 50
    
    // Now backwards
    --it; result.push_back(*it); // 40
    --it; result.push_back(*it); // 30
    --it; result.push_back(*it); // 20
    --it; result.push_back(*it); // 10
    
    std::vector<int> expected = {50, 40, 30, 20, 10};
    return check_sequence(result, expected, "Bidirectional iteration");
}

// ============================================================================
// 2. Stride Policy Tests
// ============================================================================

TEST_CASE(stride_policy_basic) {
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    
    using Iter = PolicyIterator<int, StridePolicy<int, 2>>;
    auto begin = Iter::begin(data.data(), data.data() + data.size());
    auto end = Iter::end(data.data(), data.data() + data.size());
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {0, 2, 4, 6, 8};
    return check_sequence(result, expected, "Stride-2 iteration");
}

TEST_CASE(stride_policy_stride4) {
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    
    using Iter = PolicyIterator<int, StridePolicy<int, 4>>;
    auto begin = Iter::begin(data.data(), data.data() + data.size());
    auto end = Iter::end(data.data(), data.data() + data.size());
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {0, 4, 8};
    return check_sequence(result, expected, "Stride-4 iteration");
}

TEST_CASE(stride_size_query) {
    std::vector<int> data = {1, 2, 3};
    
    using Iter = PolicyIterator<int, StridePolicy<int, 3>>;
    auto it = Iter::begin(data.data(), data.data() + data.size());
    
    ASSERT_EQ(it.strideSize(), 3, "Stride size should be queryable");
    return true;
}

// ============================================================================
// 3. Filter Policy Tests
// ============================================================================

TEST_CASE(filtering_policy_even) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8};
    
    auto is_even = [](const int& v) { return v % 2 == 0; };
    using Policy = FilterPolicy<int, decltype(is_even)>;
    
    auto begin = PolicyIterator<int, Policy>::begin(
        data.data(), data.data() + data.size(), Policy{}, is_even);
    auto end = PolicyIterator<int, Policy>::end(
        data.data(), data.data() + data.size(), Policy{}, is_even);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {2, 4, 6, 8};
    return check_sequence(result, expected, "Filter even numbers");
}

TEST_CASE(filtering_policy_positive) {
    std::vector<int> data = {-3, -2, -1, 0, 1, 2, 3};
    
    auto is_positive = [](const int& v) { return v > 0; };
    using Policy = FilterPolicy<int, decltype(is_positive)>;
    
    auto begin = PolicyIterator<int, Policy>::begin(
        data.data(), data.data() + data.size(), Policy{}, is_positive);
    auto end = PolicyIterator<int, Policy>::end(
        data.data(), data.data() + data.size(), Policy{}, is_positive);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {1, 2, 3};
    return check_sequence(result, expected, "Filter positive numbers");
}

// ============================================================================
// 4. Transform Policy Tests
// ============================================================================

TEST_CASE(transform_policy_double) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    auto doubler = [](const int& v) -> int { return v * 2; };
    using Policy = TransformPolicy<int, decltype(doubler)>;
    
    auto begin = PolicyIterator<int, Policy>::begin(
        data.data(), data.data() + data.size(), Policy{}, doubler);
    auto end = PolicyIterator<int, Policy>::end(
        data.data(), data.data() + data.size(), Policy{}, doubler);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {2, 4, 6, 8, 10};
    return check_sequence(result, expected, "Transform doubles values");
}

TEST_CASE(transform_bidirectional) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    auto doubler = [](const int& v) -> int { return v * 2; };
    using Policy = TransformPolicy<int, decltype(doubler)>;
    
    auto it = PolicyIterator<int, Policy>::begin(
        data.data(), data.data() + data.size(), Policy{}, doubler);
    
    ++it; // at 2
    ++it; // at 3
    ++it; // at 4
    ++it; // at 5
    
    ASSERT_EQ(*it, 10, "Transform at last element");
    --it;
    ASSERT_EQ(*it, 8, "Transform after decrement");
    --it;
    ASSERT_EQ(*it, 6, "Transform after second decrement");
    
    return true;
}

TEST_CASE(transform_type_conversion) {
    std::vector<int> data = {1, 2, 3};
    
    auto to_double = [](const int& v) -> double { return v * 1.5; };
    using Policy = TransformPolicy<int, decltype(to_double)>;
    
    auto begin = PolicyIterator<int, Policy>::begin(
        data.data(), data.data() + data.size(), Policy{}, to_double);
    auto end = PolicyIterator<int, Policy>::end(
        data.data(), data.data() + data.size(), Policy{}, to_double);
    
    std::vector<double> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    ASSERT_EQ(result.size(), 3u, "Result size");
    ASSERT_TRUE(std::abs(result[0] - 1.5) < 0.001, "Transform to double [0]");
    ASSERT_TRUE(std::abs(result[1] - 3.0) < 0.001, "Transform to double [1]");
    ASSERT_TRUE(std::abs(result[2] - 4.5) < 0.001, "Transform to double [2]");
    
    return true;
}

// ============================================================================
// 5. TensorStridePolicy Tests
// ============================================================================

TEST_CASE(tensor_stride_policy) {
    // 2x3 matrix in row-major order
    std::vector<int> data = {1, 2, 3, 4, 5, 6};
    
    TensorStridePolicy<int> policy({2, 3});  // 2 rows, 3 cols
    
    auto begin = PolicyIterator<int, TensorStridePolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, TensorStridePolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {1, 2, 3, 4, 5, 6};
    return check_sequence(result, expected, "Tensor iteration row-major");
}

TEST_CASE(tensor_stride_dims) {
    TensorStridePolicy<int> policy({10, 20, 30});
    
    ASSERT_EQ(policy.dims(), 3u, "dims()");
    ASSERT_EQ(policy.shape(0), 10u, "shape(0)");
    ASSERT_EQ(policy.shape(1), 20u, "shape(1)");
    ASSERT_EQ(policy.shape(2), 30u, "shape(2)");
    
    // Row-major strides: stride[i] = product of dimensions after i
    // stride[0] = 20 * 30 = 600
    // stride[1] = 30
    // stride[2] = 1
    ASSERT_EQ(policy.stride(0), 600, "stride(0) outer dim");
    ASSERT_EQ(policy.stride(1), 30, "stride(1) middle dim");
    ASSERT_EQ(policy.stride(2), 1, "stride(2) inner dim");
    
    ASSERT_EQ(policy.total(), 6000u, "total elements");
    
    return true;
}

TEST_CASE(tensor_retreat) {
    std::vector<int> data(12);
    std::iota(data.begin(), data.end(), 0);
    
    TensorStridePolicy<int> policy({3, 4});  // 3x4 matrix
    
    auto it = PolicyIterator<int, TensorStridePolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    
    // Advance to middle
    for (int i = 0; i < 6; ++i) ++it;  // Now at position 6
    ASSERT_EQ(*it, 6, "After 6 advances");
    
    // Retreat back
    --it;
    ASSERT_EQ(*it, 5, "After retreat");
    --it;
    ASSERT_EQ(*it, 4, "After second retreat");
    
    return true;
}

TEST_CASE(tensor_row_stride) {
    // Test non-contiguous memory: 3x2 matrix stored with row stride of 4
    // Memory: [0, 1, -, -, 2, 3, -, -, 4, 5, -, -]
    std::vector<int> data = {0, 1, 99, 99, 2, 3, 99, 99, 4, 5, 99, 99};
    
    // shape {3, 2}, strides {4, 1} (row stride = 4, column stride = 1)
    TensorStridePolicy<int> policy({3, 2}, {4, 1});
    
    auto begin = PolicyIterator<int, TensorStridePolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, TensorStridePolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {0, 1, 2, 3, 4, 5};
    return check_sequence(result, expected, "Non-contiguous row-major");
}

TEST_CASE(tensor_column_major) {
    // Column-major 2x3 matrix
    // Logical view:  | 0 2 4 |
    //                | 1 3 5 |
    // Memory: [0, 1, 2, 3, 4, 5] with strides {1, 2}
    std::vector<int> data = {0, 1, 2, 3, 4, 5};
    
    TensorStridePolicy<int> policy({2, 3}, {1, 2});  // row stride=1, col stride=2
    
    auto begin = PolicyIterator<int, TensorStridePolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, TensorStridePolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    // Row-major iteration over column-major storage visits: 0, 2, 4, 1, 3, 5
    std::vector<int> expected = {0, 2, 4, 1, 3, 5};
    return check_sequence(result, expected, "Column-major storage iteration");
}

TEST_CASE(tensor_indices) {
    TensorStridePolicy<int> policy({3, 4, 5});  // 3D tensor
    
    // Position 0 -> indices [0,0,0]
    auto idx = policy.currentIndices();
    ASSERT_EQ(idx[0], 0u, "Initial index[0]");
    ASSERT_EQ(idx[1], 0u, "Initial index[1]");
    ASSERT_EQ(idx[2], 0u, "Initial index[2]");
    
    // Advance 7 positions: in 3x4x5, position 7 = [0, 1, 2]
    // (0*20 + 1*5 + 2 = 7)
    for (int i = 0; i < 7; ++i) policy.advance();
    idx = policy.currentIndices();
    ASSERT_EQ(idx[0], 0u, "After 7: index[0]");
    ASSERT_EQ(idx[1], 1u, "After 7: index[1]");
    ASSERT_EQ(idx[2], 2u, "After 7: index[2]");
    
    return true;
}

// ============================================================================
// 6. Operators and Methods Tests
// ============================================================================

TEST_CASE(post_increment) {
    std::vector<int> data = {10, 20, 30};
    
    auto it = PolicyIterator<int>::begin(data.data(), data.data() + data.size());
    ASSERT_EQ(*it, 10, "Initial value");
    
    auto copy = it++;
    ASSERT_EQ(*copy, 10, "Post-increment should return copy");
    ASSERT_EQ(*it, 20, "Original iterator should be incremented");
    
    return true;
}

TEST_CASE(post_decrement) {
    std::vector<int> data = {10, 20, 30};
    
    auto it = PolicyIterator<int>::begin(data.data(), data.data() + data.size());
    ++it; // at 20
    ++it; // at 30
    ASSERT_EQ(*it, 30, "At last element");
    
    auto copy = it--;
    ASSERT_EQ(*copy, 30, "Post-decrement should return copy");
    ASSERT_EQ(*it, 20, "Original iterator should be decremented");
    
    return true;
}

TEST_CASE(operator_arrow) {
    std::vector<Point> data = {{1, 2}, {3, 4}, {5, 6}};
    
    auto it = PolicyIterator<Point>::begin(data.data(), data.data() + data.size());
    
    ASSERT_EQ(it->x, 1, "Arrow operator x");
    ASSERT_EQ(it->y, 2, "Arrow operator y");
    
    ++it;
    ASSERT_EQ(it->x, 3, "Arrow after increment x");
    ASSERT_EQ(it->y, 4, "Arrow after increment y");
    
    return true;
}

TEST_CASE(get_method) {
    std::vector<int> data = {1, 2, 3};
    
    auto it = PolicyIterator<int>::begin(data.data(), data.data() + data.size());
    
    ASSERT_EQ(it.get(), data.data(), "get() returns underlying pointer");
    ++it;
    ASSERT_EQ(it.get(), data.data() + 1, "get() after increment");
    
    return true;
}

// ============================================================================
// 7. Edge Cases
// ============================================================================

TEST_CASE(empty_range) {
    std::vector<int> data;
    
    auto begin = PolicyIterator<int>::begin(data.data(), data.data());
    auto end = PolicyIterator<int>::end(data.data(), data.data());
    
    ASSERT_TRUE(begin == end, "Empty range: begin == end");
    
    int count = 0;
    for (auto it = begin; it != end; ++it) {
        ++count;
    }
    ASSERT_EQ(count, 0, "Empty range should iterate zero times");
    
    return true;
}

TEST_CASE(single_element) {
    std::vector<int> data = {42};
    
    auto begin = PolicyIterator<int>::begin(data.data(), data.data() + data.size());
    auto end = PolicyIterator<int>::end(data.data(), data.data() + data.size());
    
    ASSERT_EQ(*begin, 42, "Single element value");
    
    auto it = begin;
    ++it;
    ASSERT_TRUE(it == end, "After increment should equal end");
    
    return true;
}

TEST_CASE(stride_exceeds_size) {
    std::vector<int> data = {1, 2};  // Size 2, stride 5
    
    using Iter = PolicyIterator<int, StridePolicy<int, 5>>;
    auto begin = Iter::begin(data.data(), data.data() + data.size());
    auto end = Iter::end(data.data(), data.data() + data.size());
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    // Should only visit element 0, then stride past end
    std::vector<int> expected = {1};
    return check_sequence(result, expected, "Stride exceeds size");
}

TEST_CASE(filter_matches_none) {
    std::vector<int> data = {1, 3, 5, 7};
    
    auto is_even = [](const int& v) { return v % 2 == 0; };
    using Policy = FilterPolicy<int, decltype(is_even)>;
    
    auto begin = PolicyIterator<int, Policy>::begin(
        data.data(), data.data() + data.size(), Policy{}, is_even);
    auto end = PolicyIterator<int, Policy>::end(
        data.data(), data.data() + data.size(), Policy{}, is_even);
    
    int count = 0;
    for (auto it = begin; it != end; ++it) {
        ++count;
    }
    
    ASSERT_EQ(count, 0, "Filter matching nothing should iterate zero times");
    return true;
}

TEST_CASE(filter_matches_all) {
    std::vector<int> data = {2, 4, 6, 8};
    
    auto is_even = [](const int& v) { return v % 2 == 0; };
    using Policy = FilterPolicy<int, decltype(is_even)>;
    
    auto begin = PolicyIterator<int, Policy>::begin(
        data.data(), data.data() + data.size(), Policy{}, is_even);
    auto end = PolicyIterator<int, Policy>::end(
        data.data(), data.data() + data.size(), Policy{}, is_even);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {2, 4, 6, 8};
    return check_sequence(result, expected, "Filter matching all");
}

TEST_CASE(self_comparison) {
    std::vector<int> data = {1, 2, 3};
    
    auto it = PolicyIterator<int>::begin(data.data(), data.data() + data.size());
    
    ASSERT_TRUE(it == it, "Iterator should equal itself");
    ASSERT_FALSE(it != it, "Iterator should not not-equal itself");
    
    return true;
}

TEST_CASE(const_data) {
    const std::vector<int> data = {1, 2, 3};
    
    auto begin = PolicyIterator<const int>::begin(data.data(), data.data() + data.size());
    auto end = PolicyIterator<const int>::end(data.data(), data.data() + data.size());
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {1, 2, 3};
    return check_sequence(result, expected, "Const data iteration");
}

// ============================================================================
// 8a. STL Algorithm Compatibility
// ============================================================================

TEST_CASE(stl_iterator_category_standard) {
    // Verify StandardPolicy claims bidirectional
    using Category = PolicyIterator<int>::iterator_category;
    static_assert(std::is_same_v<Category, std::bidirectional_iterator_tag>,
                  "StandardPolicy should be bidirectional");
    return true;
}

TEST_CASE(stl_iterator_category_stride) {
    // Verify StridePolicy is forward-only (not bidirectional)
    using Category = PolicyIterator<int, StridePolicy<int, 2>>::iterator_category;
    static_assert(std::is_same_v<Category, std::forward_iterator_tag>,
                  "StridePolicy should be forward-only (misaligned --end is wrong)");
    return true;
}

TEST_CASE(stl_iterator_category_filter) {
    auto pred = [](const int&) { return true; };
    using Policy = FilterPolicy<int, decltype(pred)>;
    using Category = PolicyIterator<int, Policy>::iterator_category;
    static_assert(std::is_same_v<Category, std::forward_iterator_tag>,
                  "FilterPolicy should be forward");
    return true;
}

TEST_CASE(stl_iterator_category_tensor) {
    using Category = PolicyIterator<int, TensorStridePolicy<int>>::iterator_category;
    static_assert(std::is_same_v<Category, std::bidirectional_iterator_tag>,
                  "TensorStridePolicy should be bidirectional");
    return true;
}

TEST_CASE(stl_distance_standard) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    auto begin = PolicyIterator<int>::begin(data.data(), data.data() + data.size());
    auto end = PolicyIterator<int>::end(data.data(), data.data() + data.size());
    
    auto dist = std::distance(begin, end);
    ASSERT_EQ(dist, 5, "std::distance should return 5");
    
    return true;
}

TEST_CASE(stl_distance_stride) {
    // Verify std::distance works with StridePolicy (forward-only)
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    
    using Iter = PolicyIterator<int, StridePolicy<int, 2>>;
    auto begin = Iter::begin(data.data(), data.data() + data.size());
    auto end = Iter::end(data.data(), data.data() + data.size());
    
    auto dist = std::distance(begin, end);
    ASSERT_EQ(dist, 5, "std::distance should return 5 for stride-2 over 10 elements");
    
    return true;
}

TEST_CASE(stl_distance_tensor) {
    // Verify std::distance works with TensorStridePolicy (bidirectional)
    std::vector<int> data(12);
    std::iota(data.begin(), data.end(), 0);
    
    TensorStridePolicy<int> policy({3, 4});  // 3x4 matrix = 12 elements
    
    auto begin = PolicyIterator<int, TensorStridePolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, TensorStridePolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    auto dist = std::distance(begin, end);
    ASSERT_EQ(dist, 12, "std::distance should return 12 for 3x4 tensor");
    
    return true;
}

// ============================================================================
// 8b. Spec Anchor Tests (Prevent Regression)
// ============================================================================

TEST_CASE(constructibility_standard_uses_factories) {
    // Verify StandardPolicy iterators are constructed via factories
    std::vector<int> data = {1, 2, 3};
    
    auto begin = PolicyIterator<int>::begin(data.data(), data.data() + data.size());
    auto end = PolicyIterator<int>::end(data.data(), data.data() + data.size());
    
    ASSERT_EQ(*begin, 1, "begin() points to first element");
    ASSERT_TRUE(begin != end, "begin != end for non-empty range");
    
    return true;
}

TEST_CASE(constructibility_filter_uses_factories) {
    // FilterPolicy iterators are constructed via factories with functor
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    auto pred = [](const int& x) { return x > 2; };
    using Policy = FilterPolicy<int, decltype(pred)>;
    
    auto begin = PolicyIterator<int, Policy>::begin(
        data.data(), data.data() + data.size(), Policy{}, pred);
    auto end = PolicyIterator<int, Policy>::end(
        data.data(), data.data() + data.size(), Policy{}, pred);
    
    ASSERT_EQ(*begin, 3, "Filter begin points to first matching element");
    ASSERT_TRUE(begin != end, "Filter begin != end");
    
    return true;
}

TEST_CASE(constructibility_transform_uses_factories) {
    // TransformPolicy iterators are constructed via factories with functor
    std::vector<int> data = {1, 2, 3};
    
    auto xform = [](const int& x) { return x * 10; };
    using Policy = TransformPolicy<int, decltype(xform)>;
    
    auto begin = PolicyIterator<int, Policy>::begin(
        data.data(), data.data() + data.size(), Policy{}, xform);
    auto end = PolicyIterator<int, Policy>::end(
        data.data(), data.data() + data.size(), Policy{}, xform);
    
    ASSERT_EQ(*begin, 10, "Transform begin returns transformed value");
    ASSERT_TRUE(begin != end, "Transform begin != end");
    
    return true;
}

TEST_CASE(constructibility_tensor_uses_factories) {
    // TensorStridePolicy iterators are constructed via factories
    std::vector<int> data = {1, 2, 3, 4, 5, 6};
    
    TensorStridePolicy<int> policy({2, 3});
    
    auto begin = PolicyIterator<int, TensorStridePolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, TensorStridePolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    ASSERT_EQ(*begin, 1, "Tensor begin points to first element");
    ASSERT_TRUE(begin != end, "Tensor begin != end");
    
    return true;
}

TEST_CASE(tensor_decrement_end_yields_last) {
    // --end should yield the last valid element (standard bidirectional expectation)
    std::vector<int> data = {10, 20, 30, 40, 50};
    
    TensorStridePolicy<int> policy({5});  // 1D tensor
    
    auto end = PolicyIterator<int, TensorStridePolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    --end;
    ASSERT_EQ(*end, 50, "Decrementing end should yield last element");
    
    --end;
    ASSERT_EQ(*end, 40, "Decrementing again should yield second-to-last");
    
    return true;
}

TEST_CASE(tensor_makeRowMajor_helper) {
    auto policy = makeRowMajor<int>({3, 4, 5});
    
    ASSERT_EQ(policy.dims(), 3u, "3D tensor");
    ASSERT_EQ(policy.shape(0), 3u, "shape[0] = 3");
    ASSERT_EQ(policy.shape(1), 4u, "shape[1] = 4");
    ASSERT_EQ(policy.shape(2), 5u, "shape[2] = 5");
    
    // Row-major strides for 3x4x5:
    // stride[2] = 1
    // stride[1] = 5
    // stride[0] = 4*5 = 20
    ASSERT_EQ(policy.stride(0), 20, "stride[0] = 4*5 = 20");
    ASSERT_EQ(policy.stride(1), 5, "stride[1] = 5");
    ASSERT_EQ(policy.stride(2), 1, "stride[2] = 1");
    
    ASSERT_EQ(policy.total(), 60u, "total = 3*4*5 = 60");
    
    return true;
}

TEST_CASE(tensor_padded_layout) {
    // Test padded/pitched layout: 3x4 matrix with row stride 8 (4 padding elements per row)
    std::vector<int> data(24, -1);  // Full allocated span
    
    // Fill logical elements only
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col) {
            data[static_cast<size_t>(row * 8 + col)] = row * 4 + col;  // 0..11
        }
    }
    
    // Shape: {3 rows, 4 cols}, Strides: {8, 1} (row stride = 8 due to padding)
    TensorStridePolicy<int> policy({3, 4}, {8, 1});
    
    auto begin = PolicyIterator<int, TensorStridePolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, TensorStridePolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    // Should read exactly the 12 logical elements in row-major order
    std::vector<int> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    ASSERT_EQ(result.size(), 12u, "Should iterate 12 logical elements");
    return check_sequence(result, expected, "Padded layout iteration");
}

// ============================================================================
// Stride1DPolicy Tests (Lightweight 1D Specialization)
// ============================================================================

TEST_CASE(stride1d_basic) {
    // Basic 1D strided iteration
    // Contract: end must equal base + count*stride
    // So buffer must be at least count*stride = 4*3 = 12 elements
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    
    // Every 3rd element starting at 0: indices 0, 3, 6, 9
    Stride1DPolicy<int> policy(4, 3);  // 4 elements, stride 3
    
    // end = base + count*stride = base + 12
    int* base = data.data();
    int* end = base + 4 * 3;  // Contract-compliant end
    
    auto it = PolicyIterator<int, Stride1DPolicy<int>>::begin(base, end, policy);
    auto endIt = PolicyIterator<int, Stride1DPolicy<int>>::end(base, end, policy);
    
    std::vector<int> result;
    for (; it != endIt; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {0, 3, 6, 9};
    return check_sequence(result, expected, "Stride1D basic iteration");
}

TEST_CASE(stride1d_column_sum) {
    // Simulate column iteration of a row-major matrix
    std::vector<int64_t> data(1000);
    std::iota(data.begin(), data.end(), 0);
    
    // 10x100 matrix, sum first column (indices 0, 100, 200, ... 900)
    constexpr size_t rows = 10;
    constexpr size_t cols = 100;
    
    Stride1DPolicy<int64_t> policy(rows, static_cast<std::ptrdiff_t>(cols));
    auto it = PolicyIterator<int64_t, Stride1DPolicy<int64_t>>::begin(
        data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int64_t, Stride1DPolicy<int64_t>>::end(
        data.data(), data.data() + data.size(), policy);
    
    int64_t sum = 0;
    for (; it != end; ++it) {
        sum += *it;
    }
    
    // Expected: 0 + 100 + 200 + ... + 900 = 100 * (0 + 9) / 2 * 10 = 4500
    int64_t expected = 4500;
    ASSERT_EQ(sum, expected, "Stride1D column sum should match manual calculation");
    return true;
}

TEST_CASE(stride1d_bidirectional) {
    // Contract: end must equal base + count*stride
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    
    Stride1DPolicy<int> policy(4, 3);
    int* base = data.data();
    int* end = base + 4 * 3;  // Contract-compliant end
    
    auto it = PolicyIterator<int, Stride1DPolicy<int>>::begin(base, end, policy);
    auto endIt = PolicyIterator<int, Stride1DPolicy<int>>::end(base, end, policy);
    
    // Forward to end
    ++it; ++it; ++it; ++it;
    ASSERT_TRUE(it == endIt, "Should be at end after 4 increments");
    
    // Backward
    --it;
    ASSERT_EQ(*it, 9, "Last element should be 9");
    --it;
    ASSERT_EQ(*it, 6, "Second-to-last should be 6");
    
    return true;
}

// ============================================================================
// Stride2DPolicy Tests (Lightweight 2D Specialization)
// ============================================================================

TEST_CASE(stride2d_basic) {
    // 3x4 matrix, row-major
    std::vector<int> data = {
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 10, 11
    };
    
    Stride2DPolicy<int> policy(3, 4);  // 3 rows, 4 cols, row-major
    auto it = PolicyIterator<int, Stride2DPolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, Stride2DPolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    std::vector<int> result;
    for (; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    return check_sequence(result, expected, "Stride2D row-major iteration");
}

TEST_CASE(tensor_column_major_traversal_correct) {
    // 3x4 matrix stored row-major, but traverse column-major
    // This demonstrates the CORRECT way to do column-major traversal:
    // Use TensorStridePolicy with permuted shape and strides.
    //
    // NOTE: Stride2DPolicy does NOT support column-major traversal because
    // it uses pointer-equality for end detection, which only works for
    // monotonic pointer advancement patterns.
    std::vector<int> data = {
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 10, 11
    };
    
    // For column-major traversal of a row-major stored matrix:
    // - Original: 3 rows x 4 cols, rowStride=4, colStride=1
    // - Permuted: shape={4 cols, 3 rows}, strides={1, 4}
    // This makes columns the outer dimension and rows the inner dimension
    TensorStridePolicy<int> policy({4, 3}, {1, 4});
    
    auto it = PolicyIterator<int, TensorStridePolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, TensorStridePolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    std::vector<int> result;
    for (; it != end; ++it) {
        result.push_back(*it);
    }
    
    // Column-major order: col0(0,4,8), col1(1,5,9), col2(2,6,10), col3(3,7,11)
    std::vector<int> expected = {0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11};
    return check_sequence(result, expected, "TensorStridePolicy column-major traversal");
}

TEST_CASE(stride2d_single_column) {
    // Iterate just one column of a matrix
    std::vector<int> data = {
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 10, 11
    };
    
    // 3 rows, 1 col, rowStride=4, colStride=1
    Stride2DPolicy<int> policy(3, 1, 4, 1);
    auto it = PolicyIterator<int, Stride2DPolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, Stride2DPolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    std::vector<int> result;
    for (; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {0, 4, 8};  // First column
    return check_sequence(result, expected, "Stride2D single column");
}

TEST_CASE(stride2d_bidirectional) {
    std::vector<int> data = {0, 1, 2, 3, 4, 5};  // 2x3 matrix
    
    Stride2DPolicy<int> policy(2, 3);
    auto it = PolicyIterator<int, Stride2DPolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, Stride2DPolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    // Go to end
    for (int i = 0; i < 6; ++i) ++it;
    ASSERT_TRUE(it == end, "Should be at end");
    
    // Walk back
    --it;
    ASSERT_EQ(*it, 5, "Last element");
    --it;
    ASSERT_EQ(*it, 4, "Second-to-last");
    --it;
    ASSERT_EQ(*it, 3, "Wrapped to previous row's last element");
    
    return true;
}

// ============================================================================
// Stride1D/Stride2D Spec Anchor Tests
// ============================================================================

TEST_CASE(stride1d_decrement_end_yields_last) {
    // --end should yield the last visited element (bidirectional requirement)
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    
    // count=4, stride=3 -> visits indices 0, 3, 6, 9 (values 0, 3, 6, 9)
    Stride1DPolicy<int> policy(4, 3);
    
    using Iter = PolicyIterator<int, Stride1DPolicy<int>>;
    auto end = Iter::end(data.data(), data.data() + data.size(), policy);
    
    --end;
    ASSERT_EQ(*end, 9, "Stride1D --end yields last visited element");
    
    --end;
    ASSERT_EQ(*end, 6, "Second --end yields second-to-last");
    
    return true;
}

TEST_CASE(stride2d_decrement_end_yields_last) {
    // --end should yield the last element (bidirectional requirement)
    std::vector<int> data = {0, 1, 2, 3, 4, 5};  // 2x3 matrix
    
    Stride2DPolicy<int> policy(2, 3);
    
    using Iter = PolicyIterator<int, Stride2DPolicy<int>>;
    auto end = Iter::end(data.data(), data.data() + data.size(), policy);
    
    --end;
    ASSERT_EQ(*end, 5, "Stride2D --end yields last element");
    
    --end;
    ASSERT_EQ(*end, 4, "Second --end yields second-to-last");
    
    return true;
}

TEST_CASE(reverse_iterator_stride1d) {
    // std::reverse_iterator must work correctly with Stride1DPolicy
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    
    // count=4, stride=3 -> visits 0, 3, 6, 9
    Stride1DPolicy<int> policy(4, 3);
    
    using Iter = PolicyIterator<int, Stride1DPolicy<int>>;
    auto begin = Iter::begin(data.data(), data.data() + data.size(), policy);
    auto end = Iter::end(data.data(), data.data() + data.size(), policy);
    
    std::vector<int> result;
    for (auto rit = std::make_reverse_iterator(end);
         rit != std::make_reverse_iterator(begin); ++rit) {
        result.push_back(*rit);
    }
    
    std::vector<int> expected = {9, 6, 3, 0};
    return check_sequence(result, expected, "Stride1D reverse iteration");
}

TEST_CASE(reverse_iterator_stride2d) {
    // std::reverse_iterator must work correctly with Stride2DPolicy
    std::vector<int> data = {0, 1, 2, 3, 4, 5};  // 2x3 matrix
    
    Stride2DPolicy<int> policy(2, 3);
    
    using Iter = PolicyIterator<int, Stride2DPolicy<int>>;
    auto begin = Iter::begin(data.data(), data.data() + data.size(), policy);
    auto end = Iter::end(data.data(), data.data() + data.size(), policy);
    
    std::vector<int> result;
    for (auto rit = std::make_reverse_iterator(end);
         rit != std::make_reverse_iterator(begin); ++rit) {
        result.push_back(*rit);
    }
    
    std::vector<int> expected = {5, 4, 3, 2, 1, 0};
    return check_sequence(result, expected, "Stride2D reverse iteration");
}

TEST_CASE(standard_decrement_end_yields_last) {
    // --end should yield the last valid element (bidirectional requirement)
    std::vector<int> data = {10, 20, 30, 40, 50};
    
    auto end = PolicyIterator<int>::end(data.data(), data.data() + data.size());
    
    --end;
    ASSERT_EQ(*end, 50, "Decrementing end should yield last element");
    
    --end;
    ASSERT_EQ(*end, 40, "Decrementing again should yield second-to-last");
    
    return true;
}

TEST_CASE(transform_decrement_end_yields_last) {
    // --end should yield the last valid element (bidirectional requirement)
    std::vector<int> data = {10, 20, 30, 40, 50};
    
    auto doubler = [](const int& x) -> int { return x * 2; };
    using Policy = TransformPolicy<int, decltype(doubler)>;
    
    auto end = PolicyIterator<int, Policy>::end(
        data.data(), data.data() + data.size(), Policy{}, doubler);
    
    --end;
    ASSERT_EQ(*end, 100, "Decrementing end should yield last element (doubled)");
    
    --end;
    ASSERT_EQ(*end, 80, "Decrementing again should yield second-to-last (doubled)");
    
    return true;
}

TEST_CASE(reverse_iterator_standard) {
    // std::reverse_iterator must work with bidirectional StandardPolicy
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    auto begin = PolicyIterator<int>::begin(data.data(), data.data() + data.size());
    auto end = PolicyIterator<int>::end(data.data(), data.data() + data.size());
    
    std::reverse_iterator<PolicyIterator<int>> rbegin(end);
    std::reverse_iterator<PolicyIterator<int>> rend(begin);
    
    std::vector<int> result;
    for (auto it = rbegin; it != rend; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {5, 4, 3, 2, 1};
    return check_sequence(result, expected, "Reverse iteration via std::reverse_iterator");
}

TEST_CASE(reverse_iterator_transform) {
    // std::reverse_iterator must work with bidirectional TransformPolicy
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    auto doubler = [](const int& x) -> int { return x * 2; };
    using Policy = TransformPolicy<int, decltype(doubler)>;
    
    auto begin = PolicyIterator<int, Policy>::begin(
        data.data(), data.data() + data.size(), Policy{}, doubler);
    auto end = PolicyIterator<int, Policy>::end(
        data.data(), data.data() + data.size(), Policy{}, doubler);
    
    std::reverse_iterator<PolicyIterator<int, Policy>> rbegin(end);
    std::reverse_iterator<PolicyIterator<int, Policy>> rend(begin);
    
    std::vector<int> result;
    for (auto it = rbegin; it != rend; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {10, 8, 6, 4, 2};
    return check_sequence(result, expected, "Reverse transform iteration");
}

// ============================================================================
// TensorIteration.h Composition Helper Tests
// ============================================================================

TEST_CASE(iterate_nd_1d_basic) {
    // 1D iteration should work and use Stride1DPolicy internally
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    int64_t sum = 0;
    iterateND(data.data(), {10}, {1}, [&](int v) { sum += v; });
    
    ASSERT_EQ(sum, 55, "1D iterateND sum");
    return true;
}

TEST_CASE(iterate_nd_1d_strided) {
    // 1D with stride > 1
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    int64_t sum = 0;
    iterateND(data.data(), {5}, {2}, [&](int v) { sum += v; });  // 1,3,5,7,9
    
    ASSERT_EQ(sum, 25, "1D strided iterateND sum");
    return true;
}

TEST_CASE(iterate_nd_2d_basic) {
    // 2D iteration should work and use Stride2DPolicy internally
    std::vector<int> data = {
        1, 2, 3,
        4, 5, 6
    };
    
    int64_t sum = 0;
    iterateND(data.data(), {2, 3}, {3, 1}, [&](int v) { sum += v; });
    
    ASSERT_EQ(sum, 21, "2D iterateND sum");
    return true;
}

TEST_CASE(iterate_nd_2d_contiguous) {
    // 2D with automatic stride computation
    std::vector<int> data(100);
    std::iota(data.begin(), data.end(), 1);  // 1..100
    
    int64_t sum = 0;
    iterateND(data.data(), {10, 10}, [&](int v) { sum += v; });
    
    ASSERT_EQ(sum, 5050, "2D contiguous iterateND sum");
    return true;
}

TEST_CASE(iterate_nd_3d_basic) {
    // 3D iteration: outer loop + Stride2DPolicy for inner 2D
    std::vector<int> data(2 * 3 * 4);
    std::iota(data.begin(), data.end(), 1);  // 1..24
    
    int64_t sum = 0;
    iterateND(data.data(), {2, 3, 4}, {12, 4, 1}, [&](int v) { sum += v; });
    
    ASSERT_EQ(sum, 300, "3D iterateND sum (1+2+...+24 = 300)");
    return true;
}

TEST_CASE(iterate_nd_3d_contiguous) {
    // 3D with automatic strides
    std::vector<int> data(2 * 3 * 4);
    std::iota(data.begin(), data.end(), 1);
    
    int64_t sum = 0;
    iterateND(data.data(), {2, 3, 4}, [&](int v) { sum += v; });
    
    ASSERT_EQ(sum, 300, "3D contiguous iterateND sum");
    return true;
}

TEST_CASE(iterate_nd_4d_basic) {
    // 4D iteration: 2 outer loops + Stride2DPolicy for inner 2D
    std::vector<int> data(2 * 2 * 3 * 4);
    std::iota(data.begin(), data.end(), 1);  // 1..48
    
    int64_t sum = 0;
    iterateND(data.data(), {2, 2, 3, 4}, [&](int v) { sum += v; });
    
    // Sum of 1..48 = 48*49/2 = 1176
    ASSERT_EQ(sum, 1176, "4D iterateND sum");
    return true;
}

TEST_CASE(iterate_nd_5d_basic) {
    // 5D to verify deep recursion works
    std::vector<int> data(2 * 2 * 2 * 3 * 4);
    std::iota(data.begin(), data.end(), 1);  // 1..96
    
    int64_t sum = 0;
    iterateND(data.data(), {2, 2, 2, 3, 4}, [&](int v) { sum += v; });
    
    // Sum of 1..96 = 96*97/2 = 4656
    ASSERT_EQ(sum, 4656, "5D iterateND sum");
    return true;
}

TEST_CASE(iterate_nd_mutation) {
    // Verify elements can be mutated
    std::vector<int> data = {1, 2, 3, 4, 5, 6};
    
    iterateND(data.data(), {2, 3}, [](int& v) { v *= 2; });
    
    std::vector<int> expected = {2, 4, 6, 8, 10, 12};
    return check_sequence(data, expected, "iterateND mutation");
}

TEST_CASE(reduce_nd_sum) {
    // Basic reduction
    std::vector<int> data(100);
    std::iota(data.begin(), data.end(), 1);
    
    auto sum = reduceND(data.data(), {10, 10}, int64_t{0}, std::plus<>{});
    
    ASSERT_EQ(sum, 5050, "reduceND sum");
    return true;
}

TEST_CASE(reduce_nd_max) {
    // Max reduction
    std::vector<int> data = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8};
    
    auto maxVal = reduceND(data.data(), {3, 4},
                           std::numeric_limits<int>::min(),
                           [](int a, int b) { return std::max(a, b); });
    
    ASSERT_EQ(maxVal, 9, "reduceND max");
    return true;
}

TEST_CASE(reduce_nd_product) {
    // Product reduction
    std::vector<int> data = {1, 2, 3, 4};
    
    auto product = reduceND(data.data(), {2, 2}, 1, std::multiplies<>{});
    
    ASSERT_EQ(product, 24, "reduceND product");
    return true;
}

TEST_CASE(reduce_nd_3d_strided) {
    // 3D reduction with explicit strides (padded layout)
    // 2x3x4 logical, but rows padded to 8
    std::vector<int> data(2 * 3 * 8, 0);
    int val = 1;
    for (size_t d = 0; d < 2; ++d) {
        for (size_t r = 0; r < 3; ++r) {
            for (size_t c = 0; c < 4; ++c) {
                data[d * 3 * 8 + r * 8 + c] = val++;
            }
        }
    }
    
    auto sum = reduceND(data.data(), {2, 3, 4}, {24, 8, 1}, int64_t{0}, std::plus<>{});
    
    ASSERT_EQ(sum, 300, "reduceND 3D strided sum (1..24)");
    return true;
}

TEST_CASE(transform_nd_negate) {
    // Transform all elements
    std::vector<int> data = {1, 2, 3, 4, 5, 6};
    
    transformND(data.data(), {2, 3}, std::negate<>{});
    
    std::vector<int> expected = {-1, -2, -3, -4, -5, -6};
    return check_sequence(data, expected, "transformND negate");
}

TEST_CASE(transform_nd_scale) {
    // Scale by constant
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0};
    
    transformND(data.data(), {2, 2}, [](double v) { return v * 2.5; });
    
    ASSERT_EQ(data[0], 2.5, "transformND scale [0]");
    ASSERT_EQ(data[1], 5.0, "transformND scale [1]");
    ASSERT_EQ(data[2], 7.5, "transformND scale [2]");
    ASSERT_EQ(data[3], 10.0, "transformND scale [3]");
    return true;
}

TEST_CASE(for_each_slice_basic) {
    // Process each slice of a 3D array
    std::vector<int> data(3 * 4 * 5);
    std::iota(data.begin(), data.end(), 1);  // 1..60
    
    std::vector<int64_t> sliceSums;
    forEachSlice(data.data(), {3, 4, 5},
        [&](std::size_t /*idx*/, int* slice) {
            auto sum = reduceND(slice, {4, 5}, int64_t{0}, std::plus<>{});
            sliceSums.push_back(sum);
        });
    
    ASSERT_EQ(sliceSums.size(), 3u, "forEachSlice count");
    // Slice 0: 1..20, sum = 210
    // Slice 1: 21..40, sum = 610
    // Slice 2: 41..60, sum = 1010
    ASSERT_EQ(sliceSums[0], 210, "Slice 0 sum");
    ASSERT_EQ(sliceSums[1], 610, "Slice 1 sum");
    ASSERT_EQ(sliceSums[2], 1010, "Slice 2 sum");
    return true;
}

#ifndef NDEBUG
TEST_CASE(iterate_nd_empty_contract) {
    // Verify empty shape is rejected
    std::vector<int> data = {1};
    
    bool caught = false;
    try {
        iterateND(data.data(), {}, {}, [](int) {});
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Empty shape should throw");
    return true;
}

TEST_CASE(iterate_nd_mismatch_contract) {
    // Verify shape/stride mismatch is rejected
    std::vector<int> data = {1, 2, 3, 4};
    
    bool caught = false;
    try {
        iterateND(data.data(), {2, 2}, {2}, [](int) {});  // 2 dims, 1 stride
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Shape/stride mismatch should throw");
    return true;
}

TEST_CASE(iterate_nd_zero_stride_contract) {
    // Verify zero stride is rejected (prevents UB in pointer arithmetic)
    std::vector<int> data = {1, 2, 3, 4};
    
    bool caught = false;
    try {
        iterateND(data.data(), {2, 2}, {0, 1}, [](int) {});  // Zero outer stride
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Zero stride should throw");
    return true;
}

TEST_CASE(iterate_nd_zero_dim_contract) {
    // Verify zero dimension is rejected
    std::vector<int> data = {1, 2, 3, 4};
    
    bool caught = false;
    try {
        iterateND(data.data(), {0, 2}, {2, 1}, [](int) {});  // Zero dimension
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Zero dimension should throw");
    return true;
}

TEST_CASE(for_each_slice_zero_stride_contract) {
    // Verify forEachSlice rejects zero outer stride
    std::vector<int> data(60);
    
    bool caught = false;
    try {
        forEachSlice(data.data(), {3, 4, 5}, {0, 5, 1},
                     [](std::size_t, int*) {});
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "forEachSlice with zero stride should throw");
    return true;
}
#endif

// ============================================================================
// 8c. Contract Violation Tests (Debug-Only - verify enforce fires)
// ============================================================================
// These tests verify that contract violations are caught in debug builds.
// In release builds (NDEBUG), enforce is a no-op, so these tests are skipped.

#ifndef NDEBUG
TEST_CASE(contract_tensor_advance_past_end) {
    // Verify: advancing past end triggers enforce
    std::vector<int> data = {1, 2, 3};
    TensorStridePolicy<int> policy({3});
    
    auto it = PolicyIterator<int, TensorStridePolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    bool caught = false;
    try {
        ++it;  // Should trigger: "Cannot advance past end"
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Advancing past end should throw in debug");
    return true;
}

TEST_CASE(contract_tensor_retreat_before_begin) {
    // Verify: retreating before begin triggers enforce
    std::vector<int> data = {1, 2, 3};
    TensorStridePolicy<int> policy({3});
    
    auto it = PolicyIterator<int, TensorStridePolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    
    bool caught = false;
    try {
        --it;  // Should trigger: "Cannot retreat before begin"
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Retreating before begin should throw in debug");
    return true;
}

TEST_CASE(contract_tensor_deref_end) {
    // Verify: dereferencing end iterator triggers enforce
    std::vector<int> data = {1, 2, 3};
    TensorStridePolicy<int> policy({3});
    
    auto it = PolicyIterator<int, TensorStridePolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    bool caught = false;
    try {
        (void)*it;  // Should trigger: "Cannot dereference end iterator"
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Dereferencing end should throw in debug");
    return true;
}

TEST_CASE(contract_tensor_offset_at_end) {
    // Verify: querying offset at end triggers enforce
    TensorStridePolicy<int> policy({3});
    policy.setToEnd();
    
    bool caught = false;
    try {
        (void)policy.currentOffset();
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Offset at end should throw in debug");
    return true;
}

TEST_CASE(contract_tensor_indices_at_end) {
    // Verify: querying indices at end triggers enforce
    TensorStridePolicy<int> policy({3});
    policy.setToEnd();
    
    bool caught = false;
    try {
        (void)policy.currentIndices();
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Indices at end should throw in debug");
    return true;
}

TEST_CASE(contract_tensor_empty_shape) {
    // Verify: empty shape triggers enforce
    bool caught = false;
    try {
        TensorStridePolicy<int> policy({});  // Empty shape
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Empty shape should throw in debug");
    return true;
}

TEST_CASE(contract_tensor_zero_dimension) {
    // Verify: zero dimension triggers enforce
    bool caught = false;
    try {
        TensorStridePolicy<int> policy({3, 0, 4});  // Zero dimension
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Zero dimension should throw in debug");
    return true;
}

TEST_CASE(contract_tensor_shape_stride_mismatch) {
    // Verify: shape/strides size mismatch triggers enforce
    bool caught = false;
    try {
        TensorStridePolicy<int> policy({3, 4}, {1});  // 2 dims, 1 stride
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Shape/stride mismatch should throw in debug");
    return true;
}

TEST_CASE(contract_stride1d_negative_stride) {
    // Verify: negative stride triggers enforce (not supported)
    bool caught = false;
    try {
        Stride1DPolicy<int> policy(10, -1);  // Negative stride
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Stride1D negative stride should throw in debug");
    return true;
}

TEST_CASE(contract_stride1d_zero_stride) {
    // Verify: zero stride triggers enforce
    bool caught = false;
    try {
        Stride1DPolicy<int> policy(10, 0);  // Zero stride
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Stride1D zero stride should throw in debug");
    return true;
}

TEST_CASE(contract_stride2d_negative_stride) {
    // Verify: negative stride triggers enforce (not supported)
    bool caught = false;
    try {
        Stride2DPolicy<int> policy(3, 4, -4, 1);  // Negative row stride
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Stride2D negative stride should throw in debug");
    return true;
}

TEST_CASE(contract_stride2d_non_monotonic) {
    // Verify: non-monotonic stride layout triggers enforce
    // rowStride < cols * colStride means wrapping could revisit addresses
    bool caught = false;
    try {
        Stride2DPolicy<int> policy(3, 4, 2, 1);  // rowStride=2 < cols*colStride=4
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Stride2D non-monotonic layout should throw in debug");
    return true;
}

TEST_CASE(contract_stride1d_span_mismatch) {
    // Verify: end - base must equal count * stride
    std::vector<int> data(10);  // Span = 10
    
    // Policy expects span = count * stride = 5 * 3 = 15
    // But we pass end = base + 10, so span mismatch
    Stride1DPolicy<int> policy(5, 3);
    
    bool caught = false;
    try {
        using Iter = PolicyIterator<int, Stride1DPolicy<int>>;
        auto end = Iter::end(data.data(), data.data() + data.size(), policy);
        (void)end;
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Stride1D span mismatch should throw in debug");
    return true;
}

TEST_CASE(contract_stride2d_span_mismatch) {
    // Verify: end - base must equal rows * rowStride
    std::vector<int> data(10);  // Span = 10
    
    // Policy expects span = rows * rowStride = 3 * 4 = 12
    // But we pass end = base + 10, so span mismatch
    Stride2DPolicy<int> policy(3, 4);  // 3 rows, 4 cols
    
    bool caught = false;
    try {
        using Iter = PolicyIterator<int, Stride2DPolicy<int>>;
        auto end = Iter::end(data.data(), data.data() + data.size(), policy);
        (void)end;
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Stride2D span mismatch should throw in debug");
    return true;
}

TEST_CASE(contract_standard_increment_past_end) {
    // Verify: standard iterator increment past end triggers enforce
    std::vector<int> data = {1};
    auto it = PolicyIterator<int>::end(data.data(), data.data() + data.size());
    
    bool caught = false;
    try {
        ++it;  // Should trigger: "Iterator past end"
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Standard increment past end should throw in debug");
    return true;
}

TEST_CASE(contract_standard_decrement_before_begin) {
    // Verify: standard iterator decrement before begin triggers enforce
    std::vector<int> data = {1, 2, 3};
    auto it = PolicyIterator<int>::begin(data.data(), data.data() + data.size());
    
    bool caught = false;
    try {
        --it;  // Should trigger: "Cannot retreat before begin"
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Standard decrement before begin should throw in debug");
    return true;
}

TEST_CASE(contract_standard_deref_end) {
    // Verify: dereferencing end iterator triggers enforce
    std::vector<int> data = {1, 2, 3};
    auto it = PolicyIterator<int>::end(data.data(), data.data() + data.size());
    
    bool caught = false;
    try {
        (void)*it;  // Should trigger: "Cannot dereference end iterator"
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Standard deref end should throw in debug");
    return true;
}

TEST_CASE(contract_transform_deref_end) {
    // Verify: dereferencing transform end iterator triggers enforce
    std::vector<int> data = {1, 2, 3};
    auto doubler = [](const int& x) -> int { return x * 2; };
    using Policy = TransformPolicy<int, decltype(doubler)>;
    
    auto it = PolicyIterator<int, Policy>::end(
        data.data(), data.data() + data.size(), Policy{}, doubler);
    
    bool caught = false;
    try {
        (void)*it;  // Should trigger: "Cannot dereference end iterator"
    } catch (const std::logic_error&) {
        caught = true;
    }
    
    ASSERT_TRUE(caught, "Transform deref end should throw in debug");
    return true;
}
#endif // NDEBUG

// ============================================================================
// 9. Exception Safety Tests
// ============================================================================

TEST_CASE(throwing_predicate) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    int call_count = 0;
    auto throwing_pred = [&call_count](const int& v) -> bool {
        ++call_count;
        if (v == 3) throw std::runtime_error("Predicate threw");
        return v % 2 == 0;
    };
    
    using Policy = FilterPolicy<int, decltype(throwing_pred)>;
    
    bool caught = false;
    try {
        auto begin = PolicyIterator<int, Policy>::begin(
            data.data(), data.data() + data.size(), Policy{}, throwing_pred);
        auto end = PolicyIterator<int, Policy>::end(
            data.data(), data.data() + data.size(), Policy{}, throwing_pred);
        
        for (auto it = begin; it != end; ++it) {
            (void)*it;
        }
    } catch (const std::runtime_error& e) {
        caught = true;
        ASSERT_TRUE(std::string(e.what()).find("Predicate threw") != std::string::npos,
                    "Should catch predicate exception");
    }
    
    ASSERT_TRUE(caught, "Exception should propagate from predicate");
    return true;
}

TEST_CASE(throwing_transformer) {
    std::vector<int> data = {1, 2, 3};
    
    auto throwing_xform = [](const int& v) -> int {
        if (v == 2) throw std::runtime_error("Transformer threw");
        return v * 2;
    };
    
    using Policy = TransformPolicy<int, decltype(throwing_xform)>;
    
    bool caught = false;
    try {
        auto begin = PolicyIterator<int, Policy>::begin(
            data.data(), data.data() + data.size(), Policy{}, throwing_xform);
        auto end = PolicyIterator<int, Policy>::end(
            data.data(), data.data() + data.size(), Policy{}, throwing_xform);
        
        for (auto it = begin; it != end; ++it) {
            (void)*it;  // Dereference triggers transformer
        }
    } catch (const std::runtime_error& e) {
        caught = true;
        ASSERT_TRUE(std::string(e.what()).find("Transformer threw") != std::string::npos,
                    "Should catch transformer exception");
    }
    
    ASSERT_TRUE(caught, "Exception should propagate from transformer");
    return true;
}

// ============================================================================
// 10. Fuzz Tests
// ============================================================================

TEST_CASE(fuzz_standard_iteration) {
    SimpleRng rng(42);
    
    for (int trial = 0; trial < 100; ++trial) {
        int size = rng.nextInt(0, 50);
        std::vector<int> data(static_cast<size_t>(size));
        std::iota(data.begin(), data.end(), 0);
        
        auto begin = PolicyIterator<int>::begin(data.data(), data.data() + data.size());
        auto end = PolicyIterator<int>::end(data.data(), data.data() + data.size());
        
        int count = 0;
        for (auto it = begin; it != end; ++it) {
            ASSERT_EQ(*it, count, "Fuzz standard iteration value mismatch");
            ++count;
        }
        ASSERT_EQ(count, size, "Fuzz standard iteration count mismatch");
    }
    
    return true;
}

TEST_CASE(fuzz_stride_iteration) {
    SimpleRng rng(43);
    
    for (int trial = 0; trial < 100; ++trial) {
        int size = rng.nextInt(0, 50);
        std::vector<int> data(static_cast<size_t>(size));
        std::iota(data.begin(), data.end(), 0);
        
        using Iter = PolicyIterator<int, StridePolicy<int, 3>>;
        auto begin = Iter::begin(data.data(), data.data() + data.size());
        auto end = Iter::end(data.data(), data.data() + data.size());
        
        int expected_idx = 0;
        for (auto it = begin; it != end; ++it) {
            ASSERT_EQ(*it, expected_idx, "Fuzz stride iteration value mismatch");
            expected_idx += 3;
        }
    }
    
    return true;
}

TEST_CASE(fuzz_filter_iteration) {
    SimpleRng rng(44);
    
    for (int trial = 0; trial < 100; ++trial) {
        int size = rng.nextInt(0, 50);
        std::vector<int> data(static_cast<size_t>(size));
        std::iota(data.begin(), data.end(), 0);
        
        auto is_even = [](const int& v) { return v % 2 == 0; };
        using Policy = FilterPolicy<int, decltype(is_even)>;
        
        auto begin = PolicyIterator<int, Policy>::begin(
            data.data(), data.data() + data.size(), Policy{}, is_even);
        auto end = PolicyIterator<int, Policy>::end(
            data.data(), data.data() + data.size(), Policy{}, is_even);
        
        int next_expected = 0;
        for (auto it = begin; it != end; ++it) {
            ASSERT_EQ(*it, next_expected, "Fuzz filter iteration value mismatch");
            next_expected += 2;
        }
    }
    
    return true;
}

// ============================================================================
// Benchmarks (Release Only)
// ============================================================================

void run_benchmarks() {
#ifdef NDEBUG
    std::cout << "\n=== PolicyIterator Benchmarks ===\n";
    
    constexpr size_t kSize = 10'000'000;
    constexpr int kRuns = 5;  // Multiple runs for stability
    std::vector<int> data(kSize);
    std::iota(data.begin(), data.end(), 0);
    
    volatile int sink = 0;
    
    // Warmup both paths
    for (int* p = data.data(); p < data.data() + kSize; ++p) sink = *p;
    auto b = PolicyIterator<int>::begin(data.data(), data.data() + data.size());
    auto e = PolicyIterator<int>::end(data.data(), data.data() + data.size());
    for (auto it = b; it != e; ++it) sink = *it;
    
    double best_raw_ns = 1e18;
    double best_policy_ns = 1e18;
    
    for (int run = 0; run < kRuns; ++run) {
        // Raw pointer
        auto start = std::chrono::high_resolution_clock::now();
        for (int* p = data.data(); p < data.data() + kSize; ++p) {
            sink = *p;
        }
        auto raw_time = std::chrono::high_resolution_clock::now() - start;
        double raw_ns = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(raw_time).count());
        best_raw_ns = std::min(best_raw_ns, raw_ns);
        
        // PolicyIterator
        auto begin = PolicyIterator<int>::begin(data.data(), data.data() + data.size());
        auto end = PolicyIterator<int>::end(data.data(), data.data() + data.size());
        start = std::chrono::high_resolution_clock::now();
        for (auto it = begin; it != end; ++it) {
            sink = *it;
        }
        auto policy_time = std::chrono::high_resolution_clock::now() - start;
        double policy_ns = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(policy_time).count());
        best_policy_ns = std::min(best_policy_ns, policy_ns);
    }
    
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  Raw pointer:   " << (best_raw_ns / static_cast<double>(kSize)) << " ns/elem\n";
    std::cout << "  PolicyIterator:" << (best_policy_ns / static_cast<double>(kSize)) << " ns/elem\n";
    std::cout << "  Overhead:      " << (best_policy_ns / best_raw_ns) << "x\n";
    
    (void)sink;
#else
    std::cout << "\n[Debug build - skipping benchmarks]\n";
#endif
}

} // namespace fat_p::testing::policyiterator

// =============================================================================
// Test Runner
// =============================================================================
namespace fat_p::testing {

bool test_PolicyIterator() {
    std::cout << "\n";
    std::cout << "================================================================================\n";
    std::cout << "  PolicyIterator Test Suite\n";
    std::cout << "  Fat-P Library - Policy-Based Iterator\n";
    std::cout << "================================================================================\n";
    
    TestRunner runner;
    
    // Standard Policy
    std::cout << "\n" << colors::blue() << "--- Standard Policy ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, standard_policy_basic);
    RUN_TEST_NS(runner, policyiterator, standard_policy_bidirectional);
    
    // Stride Policy
    std::cout << "\n" << colors::blue() << "--- Stride Policy ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, stride_policy_basic);
    RUN_TEST_NS(runner, policyiterator, stride_policy_stride4);
    RUN_TEST_NS(runner, policyiterator, stride_size_query);
    
    // Filter Policy
    std::cout << "\n" << colors::blue() << "--- Filter Policy ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, filtering_policy_even);
    RUN_TEST_NS(runner, policyiterator, filtering_policy_positive);
    
    // Transform Policy
    std::cout << "\n" << colors::blue() << "--- Transform Policy ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, transform_policy_double);
    RUN_TEST_NS(runner, policyiterator, transform_bidirectional);
    RUN_TEST_NS(runner, policyiterator, transform_type_conversion);
    
    // Tensor Stride Policy
    std::cout << "\n" << colors::blue() << "--- Tensor Stride Policy ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, tensor_stride_policy);
    RUN_TEST_NS(runner, policyiterator, tensor_stride_dims);
    RUN_TEST_NS(runner, policyiterator, tensor_retreat);
    RUN_TEST_NS(runner, policyiterator, tensor_row_stride);
    RUN_TEST_NS(runner, policyiterator, tensor_column_major);
    RUN_TEST_NS(runner, policyiterator, tensor_indices);
    
    // Stride1D/Stride2D Policies (Lightweight Specializations)
    std::cout << "\n" << colors::blue() << "--- Stride1D/Stride2D Policies ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, stride1d_basic);
    RUN_TEST_NS(runner, policyiterator, stride1d_column_sum);
    RUN_TEST_NS(runner, policyiterator, stride1d_bidirectional);
    RUN_TEST_NS(runner, policyiterator, stride2d_basic);
    RUN_TEST_NS(runner, policyiterator, stride2d_single_column);
    RUN_TEST_NS(runner, policyiterator, stride2d_bidirectional);
    RUN_TEST_NS(runner, policyiterator, tensor_column_major_traversal_correct);
    
    // Operators and Methods
    std::cout << "\n" << colors::blue() << "--- Operators & Methods ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, post_increment);
    RUN_TEST_NS(runner, policyiterator, post_decrement);
    RUN_TEST_NS(runner, policyiterator, operator_arrow);
    RUN_TEST_NS(runner, policyiterator, get_method);
    
    // Edge Cases
    std::cout << "\n" << colors::blue() << "--- Edge Cases ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, empty_range);
    RUN_TEST_NS(runner, policyiterator, single_element);
    RUN_TEST_NS(runner, policyiterator, stride_exceeds_size);
    RUN_TEST_NS(runner, policyiterator, filter_matches_none);
    RUN_TEST_NS(runner, policyiterator, filter_matches_all);
    RUN_TEST_NS(runner, policyiterator, self_comparison);
    RUN_TEST_NS(runner, policyiterator, const_data);
    
    // STL Algorithm Compatibility
    std::cout << "\n" << colors::blue() << "--- STL Algorithm Compatibility ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, stl_iterator_category_standard);
    RUN_TEST_NS(runner, policyiterator, stl_iterator_category_stride);
    RUN_TEST_NS(runner, policyiterator, stl_iterator_category_filter);
    RUN_TEST_NS(runner, policyiterator, stl_iterator_category_tensor);
    RUN_TEST_NS(runner, policyiterator, stl_distance_standard);
    RUN_TEST_NS(runner, policyiterator, stl_distance_stride);
    RUN_TEST_NS(runner, policyiterator, stl_distance_tensor);
    
    // Spec Anchor Tests
    std::cout << "\n" << colors::blue() << "--- Spec Anchor Tests ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, constructibility_standard_uses_factories);
    RUN_TEST_NS(runner, policyiterator, constructibility_filter_uses_factories);
    RUN_TEST_NS(runner, policyiterator, constructibility_transform_uses_factories);
    RUN_TEST_NS(runner, policyiterator, constructibility_tensor_uses_factories);
    RUN_TEST_NS(runner, policyiterator, tensor_decrement_end_yields_last);
    RUN_TEST_NS(runner, policyiterator, tensor_makeRowMajor_helper);
    RUN_TEST_NS(runner, policyiterator, tensor_padded_layout);
    RUN_TEST_NS(runner, policyiterator, standard_decrement_end_yields_last);
    RUN_TEST_NS(runner, policyiterator, transform_decrement_end_yields_last);
    RUN_TEST_NS(runner, policyiterator, reverse_iterator_standard);
    RUN_TEST_NS(runner, policyiterator, reverse_iterator_transform);
    RUN_TEST_NS(runner, policyiterator, stride1d_decrement_end_yields_last);
    RUN_TEST_NS(runner, policyiterator, stride2d_decrement_end_yields_last);
    RUN_TEST_NS(runner, policyiterator, reverse_iterator_stride1d);
    RUN_TEST_NS(runner, policyiterator, reverse_iterator_stride2d);
    
    // TensorIteration Composition Helpers
    std::cout << "\n" << colors::blue() << "--- TensorIteration Composition Helpers ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, iterate_nd_1d_basic);
    RUN_TEST_NS(runner, policyiterator, iterate_nd_1d_strided);
    RUN_TEST_NS(runner, policyiterator, iterate_nd_2d_basic);
    RUN_TEST_NS(runner, policyiterator, iterate_nd_2d_contiguous);
    RUN_TEST_NS(runner, policyiterator, iterate_nd_3d_basic);
    RUN_TEST_NS(runner, policyiterator, iterate_nd_3d_contiguous);
    RUN_TEST_NS(runner, policyiterator, iterate_nd_4d_basic);
    RUN_TEST_NS(runner, policyiterator, iterate_nd_5d_basic);
    RUN_TEST_NS(runner, policyiterator, iterate_nd_mutation);
    RUN_TEST_NS(runner, policyiterator, reduce_nd_sum);
    RUN_TEST_NS(runner, policyiterator, reduce_nd_max);
    RUN_TEST_NS(runner, policyiterator, reduce_nd_product);
    RUN_TEST_NS(runner, policyiterator, reduce_nd_3d_strided);
    RUN_TEST_NS(runner, policyiterator, transform_nd_negate);
    RUN_TEST_NS(runner, policyiterator, transform_nd_scale);
    RUN_TEST_NS(runner, policyiterator, for_each_slice_basic);
    
#ifndef NDEBUG
    // Contract Violation Tests (debug-only)
    std::cout << "\n" << colors::blue() << "--- Contract Violation Tests (Debug) ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, iterate_nd_empty_contract);
    RUN_TEST_NS(runner, policyiterator, iterate_nd_mismatch_contract);
    RUN_TEST_NS(runner, policyiterator, iterate_nd_zero_stride_contract);
    RUN_TEST_NS(runner, policyiterator, iterate_nd_zero_dim_contract);
    RUN_TEST_NS(runner, policyiterator, for_each_slice_zero_stride_contract);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_advance_past_end);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_retreat_before_begin);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_deref_end);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_offset_at_end);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_indices_at_end);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_empty_shape);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_zero_dimension);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_shape_stride_mismatch);
    RUN_TEST_NS(runner, policyiterator, contract_stride1d_negative_stride);
    RUN_TEST_NS(runner, policyiterator, contract_stride1d_zero_stride);
    RUN_TEST_NS(runner, policyiterator, contract_stride2d_negative_stride);
    RUN_TEST_NS(runner, policyiterator, contract_stride2d_non_monotonic);
    RUN_TEST_NS(runner, policyiterator, contract_stride1d_span_mismatch);
    RUN_TEST_NS(runner, policyiterator, contract_stride2d_span_mismatch);
    RUN_TEST_NS(runner, policyiterator, contract_standard_increment_past_end);
    RUN_TEST_NS(runner, policyiterator, contract_standard_decrement_before_begin);
    RUN_TEST_NS(runner, policyiterator, contract_standard_deref_end);
    RUN_TEST_NS(runner, policyiterator, contract_transform_deref_end);
#endif
    
    // Exception Safety
    std::cout << "\n" << colors::blue() << "--- Exception Safety ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, throwing_predicate);
    RUN_TEST_NS(runner, policyiterator, throwing_transformer);
    
    // Fuzz Tests
    std::cout << "\n" << colors::blue() << "--- Fuzz Tests ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, fuzz_standard_iteration);
    RUN_TEST_NS(runner, policyiterator, fuzz_stride_iteration);
    RUN_TEST_NS(runner, policyiterator, fuzz_filter_iteration);
    
    // Benchmarks
    policyiterator::run_benchmarks();

    return runner.print_summary() == 0;
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main() {
    return fat_p::testing::test_PolicyIterator() ? 0 : 1;
}
#endif
