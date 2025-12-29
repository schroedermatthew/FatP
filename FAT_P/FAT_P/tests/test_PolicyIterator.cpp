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
    
    PolicyIterator<int> begin(data.data(), data.data() + data.size());
    PolicyIterator<int> end(data.data() + data.size(), data.data() + data.size());
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {1, 2, 3, 4, 5};
    return check_sequence(result, expected, "Standard policy iteration");
}

TEST_CASE(standard_policy_bidirectional) {
    std::vector<int> data = {10, 20, 30, 40, 50};
    
    PolicyIterator<int> it(data.data() + 4, data.data() + data.size());
    
    std::vector<int> result;
    result.push_back(*it); // 50
    
    for (int i = 0; i < 4; ++i) {
        --it;
        result.push_back(*it);
    }
    
    std::vector<int> expected = {50, 40, 30, 20, 10};
    return check_sequence(result, expected, "Bidirectional iteration");
}

// ============================================================================
// 2. Stride Policy Tests
// ============================================================================

TEST_CASE(stride_policy_basic) {
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    
    PolicyIterator<int, StridePolicy<int, 2>> begin(data.data(), data.data() + data.size());
    PolicyIterator<int, StridePolicy<int, 2>> end(data.data() + data.size(), data.data() + data.size());
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {0, 2, 4, 6, 8};
    return check_sequence(result, expected, "Stride-2 iteration");
}

TEST_CASE(stride_policy_stride4) {
    std::array<int, 16> data;
    std::iota(data.begin(), data.end(), 0);
    
    PolicyIterator<int, StridePolicy<int, 4>> begin(data.data(), data.data() + data.size());
    PolicyIterator<int, StridePolicy<int, 4>> end(data.data() + data.size(), data.data() + data.size());
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {0, 4, 8, 12};
    return check_sequence(result, expected, "Stride-4 iteration");
}

TEST_CASE(stride_size_query) {
    std::vector<int> data = {1, 2, 3};
    
    PolicyIterator<int, StridePolicy<int, 2>> it2(data.data(), data.data() + data.size());
    PolicyIterator<int, StridePolicy<int, 8>> it8(data.data(), data.data() + data.size());
    
    ASSERT_EQ(it2.strideSize(), 2, "Stride-2 size query");
    ASSERT_EQ(it8.strideSize(), 8, "Stride-8 size query");
    
    return true;
}

// ============================================================================
// 3. Filtering Policy Tests
// ============================================================================

TEST_CASE(filtering_policy_even) {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    auto even_pred = [](const int& v) { return v % 2 == 0; };
    using Policy = FilterPolicy<int, decltype(even_pred)>;
    
    PolicyIterator<int, Policy> begin(data.data(), data.data() + data.size(), Policy{}, even_pred);
    PolicyIterator<int, Policy> end(data.data() + data.size(), data.data() + data.size(), Policy{}, even_pred);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {2, 4, 6, 8, 10};
    return check_sequence(result, expected, "Even filter iteration");
}

TEST_CASE(filtering_policy_positive) {
    std::vector<int> data = {-5, -3, 0, 2, -1, 4, 6, -2, 8};
    
    auto positive_pred = [](const int& v) { return v > 0; };
    using Policy = FilterPolicy<int, decltype(positive_pred)>;
    
    PolicyIterator<int, Policy> begin(data.data(), data.data() + data.size(), Policy{}, positive_pred);
    PolicyIterator<int, Policy> end(data.data() + data.size(), data.data() + data.size(), Policy{}, positive_pred);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {2, 4, 6, 8};
    return check_sequence(result, expected, "Positive filter iteration");
}

// ============================================================================
// 4. Transform Policy Tests
// ============================================================================

TEST_CASE(transform_policy_double) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    auto doubler = [](const int& v) -> int { return v * 2; };
    using Policy = TransformPolicy<int, decltype(doubler)>;
    
    PolicyIterator<int, Policy> begin(data.data(), data.data() + data.size(), Policy{}, doubler);
    PolicyIterator<int, Policy> end(data.data() + data.size(), data.data() + data.size(), Policy{}, doubler);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {2, 4, 6, 8, 10};
    return check_sequence(result, expected, "Transform (double) iteration");
}

TEST_CASE(transform_bidirectional) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    auto doubler = [](const int& v) -> int { return v * 2; };
    using Policy = TransformPolicy<int, decltype(doubler)>;
    
    PolicyIterator<int, Policy> it(data.data() + 4, data.data() + data.size(), Policy{}, doubler);
    
    ASSERT_EQ(*it, 10, "Transform at end");
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
    
    PolicyIterator<int, Policy> it(data.data(), data.data() + data.size(), Policy{}, to_double);
    
    double v1 = *it; ++it;
    double v2 = *it; ++it;
    double v3 = *it;
    
    ASSERT_TRUE(v1 == 1.5, "Transform int->double 1");
    ASSERT_TRUE(v2 == 3.0, "Transform int->double 2");
    ASSERT_TRUE(v3 == 4.5, "Transform int->double 3");
    
    return true;
}

// ============================================================================
// 5. TensorStridePolicy Tests
// ============================================================================

TEST_CASE(tensor_stride_policy) {
    // 3x4 matrix, iterate all 12 elements in row-major order
    std::vector<int> data(12);
    std::iota(data.begin(), data.end(), 0);  // 0, 1, 2, ..., 11
    
    TensorStridePolicy<int> policy({3, 4});  // 3 rows, 4 cols (row-major strides auto-computed)
    
    PolicyIterator<int, TensorStridePolicy<int>> begin(data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, TensorStridePolicy<int>>::makeEnd(data.data(), data.data() + data.size(), policy);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    // Should iterate all 12 elements in order: 0, 1, 2, ..., 11
    std::vector<int> expected(12);
    std::iota(expected.begin(), expected.end(), 0);
    return check_sequence(result, expected, "Tensor row-major iteration");
}

TEST_CASE(tensor_stride_dims) {
    // 10x20x30 tensor with row-major layout
    TensorStridePolicy<int> policy({10, 20, 30});
    
    ASSERT_EQ(policy.dims(), 3u, "dims() should return 3");
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
    
    PolicyIterator<int, TensorStridePolicy<int>> it(data.data(), data.data() + data.size(), policy);
    
    // Advance to position 5
    for (int i = 0; i < 5; ++i) ++it;
    ASSERT_EQ(*it, 5, "At position 5");
    
    // Retreat
    --it;
    ASSERT_EQ(*it, 4, "After retreat");
    --it;
    ASSERT_EQ(*it, 3, "After second retreat");
    
    return true;
}

TEST_CASE(tensor_row_stride) {
    // 3x4 matrix in row-major order
    std::vector<int> data = {
        0,  1,  2,  3,   // row 0
        4,  5,  6,  7,   // row 1
        8,  9, 10, 11    // row 2
    };
    
    TensorStridePolicy<int> policy({3, 4});  // 3 rows, 4 cols
    
    PolicyIterator<int, TensorStridePolicy<int>> begin(data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, TensorStridePolicy<int>>::makeEnd(data.data(), data.data() + data.size(), policy);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    // Row-major iteration: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
    std::vector<int> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    return check_sequence(result, expected, "Tensor row-major full iteration");
}

TEST_CASE(tensor_column_major) {
    // 3x4 matrix stored in row-major, iterate in column-major order
    std::vector<int> data = {
        0,  1,  2,  3,   // row 0
        4,  5,  6,  7,   // row 1
        8,  9, 10, 11    // row 2
    };
    
    // For column-major ITERATION of row-major STORAGE:
    // - Shape is {numCols, numRows} = {4, 3} (iterate 4 columns, each with 3 rows)
    // - Strides are {colStride, rowStride} = {1, 4} (col stride = 1, row stride = 4)
    TensorStridePolicy<int> policy({4, 3}, {1, 4});
    
    PolicyIterator<int, TensorStridePolicy<int>> begin(data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, TensorStridePolicy<int>>::makeEnd(data.data(), data.data() + data.size(), policy);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    // Column-major iteration: col 0 (0,4,8), col 1 (1,5,9), col 2 (2,6,10), col 3 (3,7,11)
    std::vector<int> expected = {0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11};
    return check_sequence(result, expected, "Tensor column-major iteration");
}

TEST_CASE(tensor_indices) {
    // Test that currentIndices() returns correct multi-dimensional indices
    TensorStridePolicy<int> policy({2, 3});  // 2x3 matrix
    
    // Position 0 → indices [0, 0]
    auto idx = policy.currentIndices();
    ASSERT_EQ(idx[0], 0u, "idx[0] at position 0");
    ASSERT_EQ(idx[1], 0u, "idx[1] at position 0");
    
    // Position 1 → indices [0, 1]
    policy.advance();
    idx = policy.currentIndices();
    ASSERT_EQ(idx[0], 0u, "idx[0] at position 1");
    ASSERT_EQ(idx[1], 1u, "idx[1] at position 1");
    
    // Position 3 → indices [1, 0] (row 1, col 0)
    policy.advance();  // pos 2: [0, 2]
    policy.advance();  // pos 3: [1, 0]
    idx = policy.currentIndices();
    ASSERT_EQ(idx[0], 1u, "idx[0] at position 3");
    ASSERT_EQ(idx[1], 0u, "idx[1] at position 3");
    
    return true;
}

// ============================================================================
// 6. Operator and Method Tests
// ============================================================================

TEST_CASE(post_increment) {
    std::vector<int> data = {10, 20, 30};
    
    PolicyIterator<int> it(data.data(), data.data() + data.size());
    
    auto copy = it++;
    ASSERT_EQ(*copy, 10, "Post-increment should return copy");
    ASSERT_EQ(*it, 20, "Original iterator should be incremented");
    
    return true;
}

TEST_CASE(post_decrement) {
    std::vector<int> data = {10, 20, 30};
    
    PolicyIterator<int> it(data.data() + 2, data.data() + data.size());
    ASSERT_EQ(*it, 30, "Start at last element");
    
    auto copy = it--;
    ASSERT_EQ(*copy, 30, "Post-decrement should return copy");
    ASSERT_EQ(*it, 20, "Original iterator should be decremented");
    
    return true;
}

TEST_CASE(operator_arrow) {
    std::vector<Point> data = {{1, 2}, {3, 4}, {5, 6}};
    
    PolicyIterator<Point> it(data.data(), data.data() + data.size());
    
    ASSERT_EQ(it->x, 1, "Arrow operator x");
    ASSERT_EQ(it->y, 2, "Arrow operator y");
    
    ++it;
    ASSERT_EQ(it->x, 3, "Arrow operator x after increment");
    
    return true;
}

TEST_CASE(get_method) {
    std::vector<int> data = {1, 2, 3};
    
    PolicyIterator<int> it(data.data(), data.data() + data.size());
    
    ASSERT_EQ(it.get(), data.data(), "get() returns underlying pointer");
    
    ++it;
    ASSERT_EQ(it.get(), data.data() + 1, "get() after increment");
    
    return true;
}

// ============================================================================
// 7. Edge Case Tests
// ============================================================================

TEST_CASE(empty_range) {
    std::vector<int> data;
    
    PolicyIterator<int> begin(data.data(), data.data());
    PolicyIterator<int> end(data.data(), data.data());
    
    int count = 0;
    for (auto it = begin; it != end; ++it) {
        ++count;
    }
    
    ASSERT_EQ(count, 0, "Empty range should iterate zero times");
    return true;
}

TEST_CASE(single_element) {
    std::vector<int> data = {42};
    
    PolicyIterator<int> begin(data.data(), data.data() + 1);
    PolicyIterator<int> end(data.data() + 1, data.data() + 1);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    ASSERT_EQ(result.size(), 1u, "Single element count");
    ASSERT_EQ(result[0], 42, "Single element value");
    return true;
}

TEST_CASE(stride_exceeds_size) {
    std::vector<int> data = {1, 2, 3};
    
    PolicyIterator<int, StridePolicy<int, 8>> begin(data.data(), data.data() + data.size());
    PolicyIterator<int, StridePolicy<int, 8>> end(data.data() + data.size(), data.data() + data.size());
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    ASSERT_EQ(result.size(), 1u, "Stride > size should yield one element");
    ASSERT_EQ(result[0], 1, "First element only");
    return true;
}

TEST_CASE(filter_matches_none) {
    std::vector<int> data = {1, 3, 5, 7, 9};
    
    auto even_pred = [](const int& v) { return v % 2 == 0; };
    using Policy = FilterPolicy<int, decltype(even_pred)>;
    
    PolicyIterator<int, Policy> begin(data.data(), data.data() + data.size(), Policy{}, even_pred);
    PolicyIterator<int, Policy> end(data.data() + data.size(), data.data() + data.size(), Policy{}, even_pred);
    
    int count = 0;
    for (auto it = begin; it != end; ++it) {
        ++count;
    }
    
    ASSERT_EQ(count, 0, "Filter matches none should yield zero elements");
    return true;
}

TEST_CASE(filter_matches_all) {
    std::vector<int> data = {2, 4, 6, 8};
    
    auto even_pred = [](const int& v) { return v % 2 == 0; };
    using Policy = FilterPolicy<int, decltype(even_pred)>;
    
    PolicyIterator<int, Policy> begin(data.data(), data.data() + data.size(), Policy{}, even_pred);
    PolicyIterator<int, Policy> end(data.data() + data.size(), data.data() + data.size(), Policy{}, even_pred);
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {2, 4, 6, 8};
    return check_sequence(result, expected, "Filter matches all");
}

TEST_CASE(self_comparison) {
    std::vector<int> data = {1, 2, 3};
    
    PolicyIterator<int> it(data.data(), data.data() + data.size());
    
    ASSERT_TRUE(it == it, "Iterator equals itself");
    ASSERT_FALSE(it != it, "Iterator not-not-equals itself");
    
    return true;
}

TEST_CASE(const_data) {
    const std::vector<int> data = {10, 20, 30, 40};
    
    PolicyIterator<const int> begin(data.data(), data.data() + data.size());
    PolicyIterator<const int> end(data.data() + data.size(), data.data() + data.size());
    
    std::vector<int> result;
    for (auto it = begin; it != end; ++it) {
        result.push_back(*it);
    }
    
    std::vector<int> expected = {10, 20, 30, 40};
    return check_sequence(result, expected, "Const data iteration");
}

// ============================================================================
// 8. STL Algorithm Compatibility Tests
// ============================================================================

TEST_CASE(stl_iterator_category_standard) {
    // Verify StandardPolicy claims bidirectional
    using Category = PolicyIterator<int>::iterator_category;
    static_assert(std::is_same_v<Category, std::bidirectional_iterator_tag>,
                  "StandardPolicy should be bidirectional");
    return true;
}

TEST_CASE(stl_iterator_category_stride) {
    // Verify StridePolicy claims bidirectional (not random_access)
    using Category = PolicyIterator<int, StridePolicy<int, 2>>::iterator_category;
    static_assert(std::is_same_v<Category, std::bidirectional_iterator_tag>,
                  "StridePolicy should be bidirectional (not random_access)");
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
    // Verify TensorStridePolicy claims bidirectional (not random_access)
    using Category = PolicyIterator<int, TensorStridePolicy<int>>::iterator_category;
    static_assert(std::is_same_v<Category, std::bidirectional_iterator_tag>,
                  "TensorStridePolicy should be bidirectional (not random_access)");
    return true;
}

TEST_CASE(stl_distance_standard) {
    // Verify std::distance works with StandardPolicy (bidirectional)
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    PolicyIterator<int> begin(data.data(), data.data() + data.size());
    PolicyIterator<int> end(data.data() + data.size(), data.data() + data.size());
    
    auto dist = std::distance(begin, end);
    ASSERT_EQ(dist, 5, "std::distance should return 5 for 5-element range");
    
    return true;
}

TEST_CASE(stl_distance_stride) {
    // Verify std::distance works with StridePolicy (bidirectional)
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    
    PolicyIterator<int, StridePolicy<int, 2>> begin(data.data(), data.data() + data.size());
    PolicyIterator<int, StridePolicy<int, 2>> end(data.data() + data.size(), data.data() + data.size());
    
    auto dist = std::distance(begin, end);
    ASSERT_EQ(dist, 5, "std::distance should return 5 for stride-2 over 10 elements");
    
    return true;
}

TEST_CASE(stl_distance_tensor) {
    // Verify std::distance works with TensorStridePolicy (bidirectional)
    std::vector<int> data(12);
    std::iota(data.begin(), data.end(), 0);
    
    TensorStridePolicy<int> policy({3, 4});  // 3x4 matrix = 12 elements
    
    PolicyIterator<int, TensorStridePolicy<int>> begin(data.data(), data.data() + data.size(), policy);
    auto end = PolicyIterator<int, TensorStridePolicy<int>>::makeEnd(data.data(), data.data() + data.size(), policy);
    
    auto dist = std::distance(begin, end);
    ASSERT_EQ(dist, 12, "std::distance should return 12 for 3x4 tensor");
    
    return true;
}

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
    
    PolicyIterator<int, Policy> it(data.data(), data.data() + data.size(), Policy{}, throwing_pred);
    PolicyIterator<int, Policy> end(data.data() + data.size(), data.data() + data.size(), Policy{}, throwing_pred);
    
    bool caught = false;
    try {
        while (it != end) {
            (void)*it;
            ++it;
        }
    } catch (const std::runtime_error& e) {
        caught = true;
        ASSERT_TRUE(std::string(e.what()).find("Predicate threw") != std::string::npos,
                    "Correct exception message");
    }
    
    ASSERT_TRUE(caught, "Exception was thrown and caught");
    return true;
}

TEST_CASE(throwing_transformer) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    auto throwing_transform = [](const int& v) -> int {
        if (v == 3) throw std::runtime_error("Transformer threw");
        return v * 2;
    };
    
    using Policy = TransformPolicy<int, decltype(throwing_transform)>;
    
    PolicyIterator<int, Policy> it(data.data(), data.data() + data.size(), Policy{}, throwing_transform);
    
    bool caught = false;
    try {
        for (int i = 0; i < 5; ++i) {
            (void)*it;
            ++it;
        }
    } catch (const std::runtime_error& e) {
        caught = true;
        ASSERT_TRUE(std::string(e.what()).find("Transformer threw") != std::string::npos,
                    "Correct exception message");
    }
    
    ASSERT_TRUE(caught, "Exception was thrown and caught");
    return true;
}

// ============================================================================
// 10. Fuzz Tests
// ============================================================================

TEST_CASE(fuzz_standard_iteration) {
    SimpleRng rng(42);
    
    for (int trial = 0; trial < 100; ++trial) {
        int size = rng.nextInt(0, 100);
        std::vector<int> data(static_cast<size_t>(size));
        for (int i = 0; i < size; ++i) {
            data[static_cast<size_t>(i)] = rng.nextInt(-1000, 1000);
        }
        
        PolicyIterator<int> begin(data.data(), data.data() + data.size());
        PolicyIterator<int> end(data.data() + data.size(), data.data() + data.size());
        
        int count = 0;
        for (auto it = begin; it != end; ++it) {
            ++count;
            (void)*it;
        }
        
        if (count != size) {
            ASSERT_EQ(count, size, "Fuzz standard iteration count");
            return false;
        }
    }
    
    return true;
}

TEST_CASE(fuzz_stride_iteration) {
    SimpleRng rng(123);
    
    for (int trial = 0; trial < 50; ++trial) {
        int size = rng.nextInt(1, 100);
        std::vector<int> data(static_cast<size_t>(size));
        std::iota(data.begin(), data.end(), 0);
        
        // Test with stride 2
        int stride = 2;
        int expected_count = (size + stride - 1) / stride;
        
        PolicyIterator<int, StridePolicy<int, 2>> begin(data.data(), data.data() + data.size());
        PolicyIterator<int, StridePolicy<int, 2>> end(data.data() + data.size(), data.data() + data.size());
        
        int count = 0;
        for (auto it = begin; it != end; ++it) {
            ++count;
            (void)*it;
        }
        
        if (count != expected_count) {
            ASSERT_EQ(count, expected_count, "Fuzz stride iteration count mismatch");
            return false;
        }
    }
    
    return true;
}

TEST_CASE(fuzz_filter_iteration) {
    SimpleRng rng(456);
    
    for (int trial = 0; trial < 50; ++trial) {
        int size = rng.nextInt(0, 100);
        int threshold = rng.nextInt(0, 100);
        
        std::vector<int> data(static_cast<size_t>(size));
        int expected_matches = 0;
        for (int i = 0; i < size; ++i) {
            data[static_cast<size_t>(i)] = rng.nextInt(0, 100);
            if (data[static_cast<size_t>(i)] > threshold) ++expected_matches;
        }
        
        auto pred = [threshold](const int& v) { return v > threshold; };
        using Policy = FilterPolicy<int, decltype(pred)>;
        
        PolicyIterator<int, Policy> begin(data.data(), data.data() + data.size(), Policy{}, pred);
        PolicyIterator<int, Policy> end(data.data() + data.size(), data.data() + data.size(), Policy{}, pred);
        
        int count = 0;
        for (auto it = begin; it != end; ++it) {
            ++count;
            int val = *it;
            if (val <= threshold) {
                ASSERT_TRUE(val > threshold, "Fuzz filter returned non-matching element");
                return false;
            }
        }
        
        if (count != expected_matches) {
            ASSERT_EQ(count, expected_matches, "Fuzz filter count");
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void run_benchmarks() {
#ifdef NDEBUG
    std::cout << "\n" << colors::cyan() << "=== PolicyIterator Benchmarks ===" << colors::reset() << "\n";
    
    constexpr size_t N = 1000000;
    std::vector<int> data(N);
    std::iota(data.begin(), data.end(), 0);
    
    // Raw pointer baseline
    volatile long long raw_sum = 0;
    double raw_time = measure_perf([&]() {
        for (int* ptr = data.data(); ptr < data.data() + N; ++ptr) {
            raw_sum += *ptr;
        }
    }, 10, 2);
    DoNotOptimize(raw_sum);
    
    // PolicyIterator standard
    volatile long long policy_sum = 0;
    double policy_time = measure_perf([&]() {
        PolicyIterator<int> begin(data.data(), data.data() + N);
        PolicyIterator<int> end(data.data() + N, data.data() + N);
        for (auto it = begin; it != end; ++it) {
            policy_sum += *it;
        }
    }, 10, 2);
    DoNotOptimize(policy_sum);
    
    std::cout << "  Raw pointer:   " << format_time(raw_time / static_cast<double>(N)) << "/elem\n";
    std::cout << "  PolicyIterator:" << format_time(policy_time / static_cast<double>(N)) << "/elem\n";
    std::cout << "  Overhead:      " << std::fixed << std::setprecision(2)
              << (policy_time / raw_time) << "x\n";
#else
    std::cout << "\n[Debug build - skipping benchmarks]\n";
#endif
}

} // namespace fat_p::testing::policyiterator

// ============================================================================
// Public Interface
// ============================================================================

namespace fat_p::testing {

bool test_PolicyIterator() {
    PRINT_HEADER(POLICY ITERATOR)
    
    get_test_config().verbose = true;
    auto& out = *get_test_config().output;

    TestRunner runner;

    // Standard Policy
    out << colors::blue() << "--- Standard Policy ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, standard_policy_basic);
    RUN_TEST_NS(runner, policyiterator, standard_policy_bidirectional);
    
    // Stride Policy
    out << "\n" << colors::blue() << "--- Stride Policy ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, stride_policy_basic);
    RUN_TEST_NS(runner, policyiterator, stride_policy_stride4);
    RUN_TEST_NS(runner, policyiterator, stride_size_query);
    
    // Filter Policy
    out << "\n" << colors::blue() << "--- Filter Policy ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, filtering_policy_even);
    RUN_TEST_NS(runner, policyiterator, filtering_policy_positive);
    
    // Transform Policy
    out << "\n" << colors::blue() << "--- Transform Policy ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, transform_policy_double);
    RUN_TEST_NS(runner, policyiterator, transform_bidirectional);
    RUN_TEST_NS(runner, policyiterator, transform_type_conversion);
    
    // Tensor Stride Policy
    out << "\n" << colors::blue() << "--- Tensor Stride Policy ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, tensor_stride_policy);
    RUN_TEST_NS(runner, policyiterator, tensor_stride_dims);
    RUN_TEST_NS(runner, policyiterator, tensor_retreat);
    RUN_TEST_NS(runner, policyiterator, tensor_row_stride);
    RUN_TEST_NS(runner, policyiterator, tensor_column_major);
    RUN_TEST_NS(runner, policyiterator, tensor_indices);
    
    // Operators and Methods
    out << "\n" << colors::blue() << "--- Operators & Methods ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, post_increment);
    RUN_TEST_NS(runner, policyiterator, post_decrement);
    RUN_TEST_NS(runner, policyiterator, operator_arrow);
    RUN_TEST_NS(runner, policyiterator, get_method);
    
    // Edge Cases
    out << "\n" << colors::blue() << "--- Edge Cases ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, empty_range);
    RUN_TEST_NS(runner, policyiterator, single_element);
    RUN_TEST_NS(runner, policyiterator, stride_exceeds_size);
    RUN_TEST_NS(runner, policyiterator, filter_matches_none);
    RUN_TEST_NS(runner, policyiterator, filter_matches_all);
    RUN_TEST_NS(runner, policyiterator, self_comparison);
    RUN_TEST_NS(runner, policyiterator, const_data);
    
    // STL Compatibility
    out << "\n" << colors::blue() << "--- STL Algorithm Compatibility ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, stl_iterator_category_standard);
    RUN_TEST_NS(runner, policyiterator, stl_iterator_category_stride);
    RUN_TEST_NS(runner, policyiterator, stl_iterator_category_filter);
    RUN_TEST_NS(runner, policyiterator, stl_iterator_category_tensor);
    RUN_TEST_NS(runner, policyiterator, stl_distance_standard);
    RUN_TEST_NS(runner, policyiterator, stl_distance_stride);
    RUN_TEST_NS(runner, policyiterator, stl_distance_tensor);
    
    // Exception Safety
    out << "\n" << colors::blue() << "--- Exception Safety ---" << colors::reset() << "\n";
    RUN_TEST_NS(runner, policyiterator, throwing_predicate);
    RUN_TEST_NS(runner, policyiterator, throwing_transformer);
    
    // Fuzz Tests
    out << "\n" << colors::blue() << "--- Fuzz Tests ---" << colors::reset() << "\n";
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
