/**
 * @file FixedTensor.h
 * @brief Compile-time fixed-size tensors with compile-time shape checking
 * 
 * Provides zero-overhead tensor operations with shapes known at compile time.
 * C++17 compatible.
 */

#pragma once

#include <array>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <initializer_list>

namespace cpp_utilities {

// =============================================================================
// Shape and Type Traits
// =============================================================================

template <std::size_t... Dims>
struct Shape {
    static constexpr std::size_t rank = sizeof...(Dims);
    
    // C++17 fold expression for computing size
    static constexpr std::size_t size = (Dims * ...);
    
    static constexpr std::array<std::size_t, rank> dimensions = {Dims...};
};

template <typename Shape1, typename Shape2>
struct shapes_equal : std::false_type {};

template <std::size_t... Dims1, std::size_t... Dims2>
struct shapes_equal<Shape<Dims1...>, Shape<Dims2...>> 
    : std::bool_constant<((Dims1 == Dims2) && ...)> {};

template <typename Shape1, typename Shape2>
inline constexpr bool shapes_equal_v = shapes_equal<Shape1, Shape2>::value;

template <typename Shape1, typename Shape2>
struct matmul_compatible : std::false_type {};

template <std::size_t M, std::size_t N, std::size_t P>
struct matmul_compatible<Shape<M, N>, Shape<N, P>> : std::true_type {};

template <typename Shape1, typename Shape2>
inline constexpr bool matmul_compatible_v = matmul_compatible<Shape1, Shape2>::value;

template <typename Shape1, typename Shape2>
struct matmul_result_shape;

template <std::size_t M, std::size_t N, std::size_t P>
struct matmul_result_shape<Shape<M, N>, Shape<N, P>> {
    using type = Shape<M, P>;
};

template <typename Shape1, typename Shape2>
using matmul_result_shape_t = typename matmul_result_shape<Shape1, Shape2>::type;

// =============================================================================
// Policies
// =============================================================================

struct UncheckedArithmetic {};
struct CheckedArithmetic {};

// =============================================================================
// FixedTensor Class
// =============================================================================

template <typename T, std::size_t... Dims>
class FixedTensor {
public:
    using shape_type = Shape<Dims...>;
    using value_type = T;
    static constexpr std::size_t rank = shape_type::rank;
    static constexpr std::size_t size = shape_type::size;
    static constexpr std::array<std::size_t, rank> shape = {Dims...};
    
    using iterator = typename std::array<T, size>::iterator;
    using const_iterator = typename std::array<T, size>::const_iterator;
    
private:
    std::array<T, size> data_;
    
    // Helper to compute flat index from multi-dimensional indices
    template <typename... Indices>
    static constexpr std::size_t compute_index(std::size_t first, Indices... rest) {
        if constexpr (sizeof...(rest) == 0) {
            return first;
        } else {
            constexpr std::size_t dims[] = {Dims...};
            constexpr std::size_t current_rank = rank - sizeof...(rest) - 1;
            std::size_t stride = 1;
            for (std::size_t i = current_rank + 1; i < rank; ++i) {
                stride *= dims[i];
            }
            return first * stride + compute_index(rest...);
        }
    }
    
public:
    // =========================================================================
    // Constructors
    // =========================================================================
    
    FixedTensor() : data_{} {}
    
    explicit FixedTensor(T value) { 
        data_.fill(value); 
    }
    
    FixedTensor(std::initializer_list<T> init) {
        if (init.size() != size) {
            throw std::invalid_argument("Initializer list size mismatch");
        }
        std::copy(init.begin(), init.end(), data_.begin());
    }
    
    explicit FixedTensor(const std::array<T, size>& arr) : data_(arr) {}
    
    // =========================================================================
    // Element Access (C++17 compatible - using SFINAE instead of requires)
    // =========================================================================
    
    // Multi-dimensional access
    template <typename... Indices, 
              typename = std::enable_if_t<sizeof...(Indices) == rank>>
    T& operator()(Indices... indices) {
        return data_[compute_index(static_cast<std::size_t>(indices)...)];
    }
    
    template <typename... Indices,
              typename = std::enable_if_t<sizeof...(Indices) == rank>>
    const T& operator()(Indices... indices) const {
        return data_[compute_index(static_cast<std::size_t>(indices)...)];
    }
    
    // Flat access
    T& operator[](std::size_t index) { 
        return data_[index]; 
    }
    
    const T& operator[](std::size_t index) const { 
        return data_[index]; 
    }
    
    // =========================================================================
    // Iterators
    // =========================================================================
    
    iterator begin() { return data_.begin(); }
    iterator end() { return data_.end(); }
    const_iterator begin() const { return data_.begin(); }
    const_iterator end() const { return data_.end(); }
    const_iterator cbegin() const { return data_.begin(); }
    const_iterator cend() const { return data_.end(); }
    
    // =========================================================================
    // Properties
    // =========================================================================
    
    constexpr std::size_t get_size() const { return size; }
    constexpr std::size_t get_rank() const { return rank; }
    const std::array<T, size>& data() const { return data_; }
    
    // =========================================================================
    // Element-wise Operations
    // =========================================================================
    
    FixedTensor operator+(const FixedTensor& other) const {
        FixedTensor result;
        for (std::size_t i = 0; i < size; ++i) {
            result.data_[i] = data_[i] + other.data_[i];
        }
        return result;
    }
    
    FixedTensor operator-(const FixedTensor& other) const {
        FixedTensor result;
        for (std::size_t i = 0; i < size; ++i) {
            result.data_[i] = data_[i] - other.data_[i];
        }
        return result;
    }
    
    FixedTensor operator*(const FixedTensor& other) const {
        FixedTensor result;
        for (std::size_t i = 0; i < size; ++i) {
            result.data_[i] = data_[i] * other.data_[i];
        }
        return result;
    }
    
    FixedTensor operator/(const FixedTensor& other) const {
        FixedTensor result;
        for (std::size_t i = 0; i < size; ++i) {
            result.data_[i] = data_[i] / other.data_[i];
        }
        return result;
    }
    
    FixedTensor operator*(T scalar) const {
        FixedTensor result;
        for (std::size_t i = 0; i < size; ++i) {
            result.data_[i] = data_[i] * scalar;
        }
        return result;
    }
    
    FixedTensor operator/(T scalar) const {
        FixedTensor result;
        for (std::size_t i = 0; i < size; ++i) {
            result.data_[i] = data_[i] / scalar;
        }
        return result;
    }
    
    FixedTensor operator+(T scalar) const {
        FixedTensor result;
        for (std::size_t i = 0; i < size; ++i) {
            result.data_[i] = data_[i] + scalar;
        }
        return result;
    }
    
    FixedTensor operator-(T scalar) const {
        FixedTensor result;
        for (std::size_t i = 0; i < size; ++i) {
            result.data_[i] = data_[i] - scalar;
        }
        return result;
    }
    
    // =========================================================================
    // In-place Operations
    // =========================================================================
    
    FixedTensor& operator+=(const FixedTensor& other) {
        for (std::size_t i = 0; i < size; ++i) {
            data_[i] += other.data_[i];
        }
        return *this;
    }
    
    FixedTensor& operator-=(const FixedTensor& other) {
        for (std::size_t i = 0; i < size; ++i) {
            data_[i] -= other.data_[i];
        }
        return *this;
    }
    
    FixedTensor& operator*=(T scalar) {
        for (std::size_t i = 0; i < size; ++i) {
            data_[i] *= scalar;
        }
        return *this;
    }
    
    FixedTensor& operator/=(T scalar) {
        for (std::size_t i = 0; i < size; ++i) {
            data_[i] /= scalar;
        }
        return *this;
    }
    
    // =========================================================================
    // Comparison Operations
    // =========================================================================
    
    bool operator==(const FixedTensor& other) const {
        return data_ == other.data_;
    }
    
    bool operator!=(const FixedTensor& other) const {
        return data_ != other.data_;
    }
    
    // =========================================================================
    // Reduction Operations
    // =========================================================================
    
    T sum() const {
        return std::accumulate(data_.begin(), data_.end(), T{0});
    }
    
    T mean() const {
        return sum() / static_cast<T>(size);
    }
    
    T max() const {
        return *std::max_element(data_.begin(), data_.end());
    }
    
    T min() const {
        return *std::min_element(data_.begin(), data_.end());
    }
};

// =============================================================================
// Matrix Operations
// =============================================================================

template <typename T, std::size_t M, std::size_t N, std::size_t P>
FixedTensor<T, M, P> matmul(const FixedTensor<T, M, N>& A, 
                             const FixedTensor<T, N, P>& B) {
    FixedTensor<T, M, P> result;
    
    for (std::size_t i = 0; i < M; ++i) {
        for (std::size_t j = 0; j < P; ++j) {
            T sum = T{0};
            for (std::size_t k = 0; k < N; ++k) {
                sum += A(i, k) * B(k, j);
            }
            result(i, j) = sum;
        }
    }
    
    return result;
}

// =============================================================================
// Vector Operations
// =============================================================================

template <typename T, std::size_t N>
T dot(const FixedTensor<T, N>& a, const FixedTensor<T, N>& b) {
    T sum = T{0};
    for (std::size_t i = 0; i < N; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

template <typename T, std::size_t N>
T norm(const FixedTensor<T, N>& v) {
    T sum_sq = T{0};
    for (std::size_t i = 0; i < N; ++i) {
        sum_sq += v[i] * v[i];
    }
    return std::sqrt(sum_sq);
}

template <typename T, std::size_t N>
FixedTensor<T, N> normalize(const FixedTensor<T, N>& v) {
    T n = norm(v);
    if (n == T{0}) {
        throw std::runtime_error("Cannot normalize zero vector");
    }
    return v / n;
}

// =============================================================================
// Type Aliases
// =============================================================================

template <typename T>
using FixedVec2 = FixedTensor<T, 2>;

template <typename T>
using FixedVec3 = FixedTensor<T, 3>;

template <typename T>
using FixedVec4 = FixedTensor<T, 4>;

using FixedVec2f = FixedVec2<float>;
using FixedVec2d = FixedVec2<double>;
using FixedVec3f = FixedVec3<float>;
using FixedVec3d = FixedVec3<double>;
using FixedVec4f = FixedVec4<float>;
using FixedVec4d = FixedVec4<double>;

template <typename T>
using FixedMat2x2 = FixedTensor<T, 2, 2>;

template <typename T>
using FixedMat3x3 = FixedTensor<T, 3, 3>;

template <typename T>
using FixedMat4x4 = FixedTensor<T, 4, 4>;

using FixedMat2x2f = FixedMat2x2<float>;
using FixedMat2x2d = FixedMat2x2<double>;
using FixedMat3x3f = FixedMat3x3<float>;
using FixedMat3x3d = FixedMat3x3<double>;
using FixedMat4x4f = FixedMat4x4<float>;
using FixedMat4x4d = FixedMat4x4<double>;

template <typename T, std::size_t N>
using FixedVecChecked = FixedTensor<T, N>;

template <typename T, std::size_t M, std::size_t N>
using FixedMatChecked = FixedTensor<T, M, N>;

template <typename T, size_t... Dims>
struct is_fixed_tensor<FixedTensor<T, Dims...>> : std::true_type {};

} // namespace cpp_utilities
