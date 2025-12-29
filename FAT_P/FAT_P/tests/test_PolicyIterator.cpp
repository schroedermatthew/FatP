/**
 * @file test_PolicyIterator.cpp
 * @brief Comprehensive unit tests for PolicyIterator.h and TensorStridePolicy.h
 *
 * Build commands:
 *   Standard:   g++ -std=c++17 -DENABLE_TEST_APPLICATION -I. -o test test_PolicyIterator.cpp
 *   Sanitizers: g++ -std=c++17 -DENABLE_TEST_APPLICATION -fsanitize=address,undefined -g -I. -o test test_PolicyIterator.cpp
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
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    
    // Every 3rd element starting at 0
    Stride1DPolicy<int> policy(4, 3);  // 4 elements, stride 3
    auto it = PolicyIterator<int, Stride1DPolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, Stride1DPolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    std::vector<int> result;
    for (; it != end; ++it) {
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
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    
    Stride1DPolicy<int> policy(4, 3);
    auto it = PolicyIterator<int, Stride1DPolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, Stride1DPolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    // Forward to end
    ++it; ++it; ++it; ++it;
    ASSERT_TRUE(it == end, "Should be at end after 4 increments");
    
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

TEST_CASE(stride2d_column_major_traversal) {
    // 3x4 matrix stored row-major, but traverse column-major
    std::vector<int> data = {
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 10, 11
    };
    
    // Traverse columns first: 4 columns, 3 rows each
    // colStride=1, rowStride=4
    Stride2DPolicy<int> policy(4, 3, 1, 4);  // 4 cols (outer), 3 rows (inner)
    auto it = PolicyIterator<int, Stride2DPolicy<int>>::begin(
        data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, Stride2DPolicy<int>>::end(
        data.data(), data.data() + data.size(), policy);
    
    std::vector<int> result;
    for (; it != end; ++it) {
        result.push_back(*it);
    }
    
    // Column-major order: col0(0,4,8), col1(1,5,9), col2(2,6,10), col3(3,7,11)
    std::vector<int> expected = {0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11};
    return check_sequence(result, expected, "Stride2D column-major traversal");
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
    
#ifndef NDEBUG
    // Contract Violation Tests (debug-only)
    std::cout << "\n" << colors::blue() << "--- Contract Violation Tests (Debug) ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, contract_tensor_advance_past_end);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_retreat_before_begin);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_deref_end);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_offset_at_end);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_indices_at_end);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_empty_shape);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_zero_dimension);
    RUN_TEST_NS(runner, policyiterator, contract_tensor_shape_stride_mismatch);
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
