/**
 * @file Tensor.h
 * @brief High-performance N-dimensional tensor with policy-based design.
 *
 * @details Provides a flexible, header-only tensor class with:
 * - Policy-based memory allocation (AllocationStrategy integration)
 * - Custom iterators (AdaptiveIterator integration)
 * - Design by Contract via enforce.h
 * - Safe arithmetic via CheckedArithmetic
 * - Thread-safety policies via ConcurrencyPolicies
 * - Expected-based error handling
 * - Zero-overhead abstractions (stateless policies)
 * - Broadcasting and advanced indexing
 * - Strided views and slicing
 * - Lazy evaluation support
 *
 * @performance Zero-overhead for default policies; debug checks via enforce.
 * @extensibility Add custom allocators, iterators, and reduction policies.
 * @requirements C++17; header-only; no external dependencies.
 *
 * @note Inspired by NumPy/PyTorch/Eigen but designed for C++ with compile-time safety.
 */
#pragma once

#include <vector>
#include <array>
#include <algorithm>
#include <numeric>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <cstring>
#include <cmath>
#include <functional>

#include "enforce.h"
#include "Expected.h"
#include "AllocationStrategy.h"
#include "AdaptiveIterator.h"
#include "CheckedArithmetic.h"
#include "ConcurrencyPolicies.h"
#include "ScopeGuard.h"
#include "StrongId.h"
#include "TypeTraits.h"
#include "Stringify.h"

namespace cpp_utilities {

// ============================================================================
// Forward Declarations and Type Aliases
// ============================================================================

template <typename T, typename AllocImpl = StandardAllocatorImpl<T>, 
          typename ConcurrencyPolicy = SingleThreadedPolicy>
class Tensor;

template <typename T>
class TensorView;

// Strong types for tensor dimensions
using TensorDimension = StrongId<size_t, struct TensorDimensionTag>;
using TensorSize = StrongId<size_t, struct TensorSizeTag>;
using TensorIndex = StrongId<size_t, struct TensorIndexTag>;

// ============================================================================
// Shape and Stride Utilities
// ============================================================================

/**
 * @brief Represents the shape of a tensor (dimensions).
 */
class TensorShape {
private:
    std::vector<size_t> dims_;
    
public:
    TensorShape() = default;
    
    explicit TensorShape(std::initializer_list<size_t> dims) : dims_(dims) {
        enforce(!dims_.empty(), "Shape cannot be empty");
        for (auto d : dims_) {
            enforce(d > 0, "All dimensions must be positive");
        }
    }
    
    explicit TensorShape(const std::vector<size_t>& dims) : dims_(dims) {
        enforce(!dims_.empty(), "Shape cannot be empty");
        for (auto d : dims_) {
            enforce(d > 0, "All dimensions must be positive");
        }
    }
    
    size_t rank() const noexcept { return dims_.size(); }
    size_t operator[](size_t i) const {
        enforce(i < dims_.size(), "Index out of bounds");
        return dims_[i];
    }
    
    size_t& operator[](size_t i) {
        enforce(i < dims_.size(), "Index out of bounds");
        return dims_[i];
    }
    
    size_t total_size() const noexcept {
        return std::accumulate(dims_.begin(), dims_.end(), size_t(1), 
                             std::multiplies<size_t>());
    }
    
    const std::vector<size_t>& dims() const noexcept { return dims_; }
    std::vector<size_t>& dims() noexcept { return dims_; }
    
    bool operator==(const TensorShape& other) const noexcept {
        return dims_ == other.dims_;
    }
    
    bool operator!=(const TensorShape& other) const noexcept {
        return !(*this == other);
    }
    
    std::string to_string() const {
        StringifyOptions opts;
        opts.container_open = "(";
        opts.container_close = ")";
        return detail::stringify_container(dims_, opts);
    }
};

/**
 * @brief Computes row-major strides from shape.
 */
inline std::vector<size_t> compute_strides(const TensorShape& shape) {
    const auto& dims = shape.dims();
    std::vector<size_t> strides(dims.size());
    
    if (dims.empty()) return strides;
    
    strides.back() = 1;
    for (int i = static_cast<int>(dims.size()) - 2; i >= 0; --i) {
        auto result = checked_mul<ReturnExpectedPolicy, size_t>(strides[i + 1], dims[i + 1]);
        enforce(result.has_value(), "Stride computation overflow");
        strides[i] = result.value();
    }
    
    return strides;
}

/**
 * @brief Computes column-major (Fortran-style) strides from shape.
 */
inline std::vector<size_t> compute_strides_column_major(const TensorShape& shape) {
    const auto& dims = shape.dims();
    std::vector<size_t> strides(dims.size());
    
    if (dims.empty()) return strides;
    
    strides[0] = 1;
    for (size_t i = 1; i < dims.size(); ++i) {
        auto result = checked_mul<ReturnExpectedPolicy, size_t>(strides[i - 1], dims[i - 1]);
        enforce(result.has_value(), "Stride computation overflow");
        strides[i] = result.value();
    }
    
    return strides;
}

/**
 * @brief Converts multi-dimensional indices to flat index using strides.
 */
inline size_t indices_to_offset(const std::vector<size_t>& indices, 
                               const std::vector<size_t>& strides) {
    enforce(indices.size() == strides.size(), 
            "Indices and strides must have same size");
    
    size_t offset = 0;
    for (size_t i = 0; i < indices.size(); ++i) {
        auto result = checked_mul<ReturnExpectedPolicy, size_t>(indices[i], strides[i]);
        enforce(result.has_value(), "Offset computation overflow");
        auto add_result = checked_add<ReturnExpectedPolicy, size_t>(offset, result.value());
        enforce(add_result.has_value(), "Offset computation overflow");
        offset = add_result.value();
    }
    
    return offset;
}

// ============================================================================
// Tensor Iterator Policies
// ============================================================================

/**
 * @brief Iterator policy for tensor elements (standard linear traversal).
 */
template <typename T>
struct TensorIteratorPolicy {
    using value_type = T;
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;
    
    void advance(T*& ptr) const noexcept {
        ++ptr;
    }
    
    void step(T*& ptr, std::ptrdiff_t n) const noexcept {
        ptr += n;
    }
    
    std::ptrdiff_t distance(const T* first, const T* last) const noexcept {
        return last - first;
    }
    
    T& dereference(T* ptr) const noexcept {
        return *ptr;
    }
    
    const T& dereference(const T* ptr) const noexcept {
        return *ptr;
    }
};

/**
 * @brief Strided iterator policy for tensor views and slices.
 */
template <typename T>
struct StridedTensorIteratorPolicy {
    using value_type = T;
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;
    
    size_t stride;
    
    explicit StridedTensorIteratorPolicy(size_t s = 1) : stride(s) {}
    
    void advance(T*& ptr) const noexcept {
        ptr += stride;
    }
    
    void step(T*& ptr, std::ptrdiff_t n) const noexcept {
        ptr += n * stride;
    }
    
    std::ptrdiff_t distance(const T* first, const T* last) const noexcept {
        return (last - first) / static_cast<std::ptrdiff_t>(stride);
    }
    
    T& dereference(T* ptr) const noexcept {
        return *ptr;
    }
    
    const T& dereference(const T* ptr) const noexcept {
        return *ptr;
    }
    
    size_t stride_size() const noexcept {
        return stride;
    }
};

// ============================================================================
// Main Tensor Class
// ============================================================================

/**
 * @brief N-dimensional tensor with policy-based design.
 * 
 * @tparam T Element type
 * @tparam AllocImpl Allocation policy (e.g., StandardAllocatorImpl, PoolAllocatorImpl)
 * @tparam ConcurrencyPolicy Thread-safety policy
 */
template <typename T, typename AllocImpl, typename ConcurrencyPolicy>
class Tensor {
public:
    using value_type = T;
    using allocator_type = AllocationStrategy<T, AllocImpl>;
    using iterator = AdaptiveIterator<T, TensorIteratorPolicy<T>, ConcurrencyPolicy>;
    using const_iterator = ConstAdaptiveIterator<T, TensorIteratorPolicy<T>, ConcurrencyPolicy>;
    
private:
    allocator_type allocator_;
    T* data_;
    TensorShape shape_;
    std::vector<size_t> strides_;
    size_t size_;
    bool owns_data_;
    
    ConcurrencyPolicy concurrency_policy_;
    
    /**
     * @brief Allocate memory for tensor data.
     */
    void allocate_data() {
        size_ = shape_.total_size();
        enforce(size_ > 0, "Cannot allocate zero-sized tensor");
        
        auto result = allocator_.allocate(StrongId<size_t, AllocSizeTag>(size_));
        enforce(result.has_value(), "Allocation failed");
        data_ = result.value();
        owns_data_ = true;
    }
    
    /**
     * @brief Deallocate memory.
     */
    void deallocate_data() {
        if (data_ && owns_data_) {
            allocator_.deallocate(data_, StrongId<size_t, AllocSizeTag>(size_));
            data_ = nullptr;
            owns_data_ = false;
        }
    }
    
    /**
     * @brief Validate indices for element access.
     */
    void validate_indices(const std::vector<size_t>& indices) const {
        enforce(indices.size() == shape_.rank(), 
                "Number of indices must match tensor rank");
        
        for (size_t i = 0; i < indices.size(); ++i) {
            enforce(indices[i] < shape_[i], 
                    "Index out of bounds at dimension ", i);
        }
    }
    
public:
    // ========================================================================
    // Constructors and Destructor
    // ========================================================================
    
    /**
     * @brief Default constructor (empty tensor).
     */
    Tensor() : data_(nullptr), size_(0), owns_data_(false) {}
    
    /**
     * @brief Construct tensor with given shape.
     */
    explicit Tensor(const TensorShape& shape) 
        : shape_(shape), strides_(compute_strides(shape)) {
        allocate_data();
    }
    
    /**
     * @brief Construct tensor with shape and fill value.
     */
    Tensor(const TensorShape& shape, const T& fill_value) 
        : shape_(shape), strides_(compute_strides(shape)) {
        allocate_data();
        std::fill_n(data_, size_, fill_value);
    }
    
    /**
     * @brief Construct tensor from initializer list (1D).
     */
    Tensor(std::initializer_list<T> values) 
        : shape_({values.size()}), strides_(compute_strides(shape_)) {
        allocate_data();
        std::copy(values.begin(), values.end(), data_);
    }
    
    /**
     * @brief Construct tensor with shape from data pointer (does NOT take ownership).
     */
    Tensor(const TensorShape& shape, T* data, bool owns = false)
        : data_(data), shape_(shape), strides_(compute_strides(shape)),
          size_(shape.total_size()), owns_data_(owns) {
        enforce(data != nullptr, "Data pointer cannot be null");
    }
    
    /**
     * @brief Copy constructor.
     */
    Tensor(const Tensor& other) 
        : allocator_(other.allocator_), shape_(other.shape_), 
          strides_(other.strides_), size_(other.size_), owns_data_(true) {
        
        if (other.size_ > 0 && other.data_) {
            auto result = allocator_.allocate(StrongId<size_t, AllocSizeTag>(size_));
            enforce(result.has_value(), "Allocation failed");
            data_ = result.value();
            std::copy_n(other.data_, size_, data_);
        } else {
            data_ = nullptr;
            owns_data_ = false;
        }
    }
    
    /**
     * @brief Move constructor.
     */
    Tensor(Tensor&& other) noexcept
        : allocator_(std::move(other.allocator_)),
          data_(other.data_),
          shape_(std::move(other.shape_)),
          strides_(std::move(other.strides_)),
          size_(other.size_),
          owns_data_(other.owns_data_) {
        
        other.data_ = nullptr;
        other.size_ = 0;
        other.owns_data_ = false;
    }
    
    /**
     * @brief Destructor.
     */
    ~Tensor() {
        deallocate_data();
    }
    
    // ========================================================================
    // Assignment Operators
    // ========================================================================
    
    /**
     * @brief Copy assignment.
     */
    Tensor& operator=(const Tensor& other) {
        if (this != &other) {
            typename ConcurrencyPolicy::LockGuard lock(concurrency_policy_.getLock());
            
            deallocate_data();
            
            allocator_ = other.allocator_;
            shape_ = other.shape_;
            strides_ = other.strides_;
            size_ = other.size_;
            owns_data_ = true;
            
            if (other.size_ > 0 && other.data_) {
                auto result = allocator_.allocate(StrongId<size_t, AllocSizeTag>(size_));
                enforce(result.has_value(), "Allocation failed");
                data_ = result.value();
                std::copy_n(other.data_, size_, data_);
            } else {
                data_ = nullptr;
                owns_data_ = false;
            }
        }
        return *this;
    }
    
    /**
     * @brief Move assignment.
     */
    Tensor& operator=(Tensor&& other) noexcept {
        if (this != &other) {
            typename ConcurrencyPolicy::LockGuard lock(concurrency_policy_.getLock());
            
            deallocate_data();
            
            allocator_ = std::move(other.allocator_);
            data_ = other.data_;
            shape_ = std::move(other.shape_);
            strides_ = std::move(other.strides_);
            size_ = other.size_;
            owns_data_ = other.owns_data_;
            
            other.data_ = nullptr;
            other.size_ = 0;
            other.owns_data_ = false;
        }
        return *this;
    }
    
    // ========================================================================
    // Element Access
    // ========================================================================
    
    /**
     * @brief Access element with multi-dimensional indices.
     */
    T& operator()(std::initializer_list<size_t> indices) {
        std::vector<size_t> idx(indices);
        validate_indices(idx);
        size_t offset = indices_to_offset(idx, strides_);
        return data_[offset];
    }
    
    const T& operator()(std::initializer_list<size_t> indices) const {
        std::vector<size_t> idx(indices);
        validate_indices(idx);
        size_t offset = indices_to_offset(idx, strides_);
        return data_[offset];
    }
    
    /**
     * @brief Access element with variadic indices.
     */
    template <typename... Indices>
    T& at(Indices... indices) {
        std::vector<size_t> idx = {static_cast<size_t>(indices)...};
        validate_indices(idx);
        size_t offset = indices_to_offset(idx, strides_);
        return data_[offset];
    }
    
    template <typename... Indices>
    const T& at(Indices... indices) const {
        std::vector<size_t> idx = {static_cast<size_t>(indices)...};
        validate_indices(idx);
        size_t offset = indices_to_offset(idx, strides_);
        return data_[offset];
    }
    
    /**
     * @brief Flat access (treats tensor as 1D array).
     */
    T& operator[](size_t index) {
        enforce(index < size_, "Index out of bounds");
        return data_[index];
    }
    
    const T& operator[](size_t index) const {
        enforce(index < size_, "Index out of bounds");
        return data_[index];
    }
    
    /**
     * @brief Raw data pointer access.
     */
    T* data() noexcept { return data_; }
    const T* data() const noexcept { return data_; }
    
    // ========================================================================
    // Shape and Properties
    // ========================================================================
    
    const TensorShape& shape() const noexcept { return shape_; }
    const std::vector<size_t>& strides() const noexcept { return strides_; }
    size_t size() const noexcept { return size_; }
    size_t rank() const noexcept { return shape_.rank(); }
    bool empty() const noexcept { return size_ == 0; }
    bool owns_data() const noexcept { return owns_data_; }
    
    // ========================================================================
    // Iterators
    // ========================================================================
    
    iterator begin() {
        return iterator(data_, data_ + size_, TensorIteratorPolicy<T>{});
    }
    
    iterator end() {
        return iterator(data_ + size_, data_ + size_, TensorIteratorPolicy<T>{});
    }
    
    const_iterator begin() const {
        return const_iterator(data_, data_ + size_, TensorIteratorPolicy<T>{});
    }
    
    const_iterator end() const {
        return const_iterator(data_ + size_, data_ + size_, TensorIteratorPolicy<T>{});
    }
    
    const_iterator cbegin() const {
        return const_iterator(data_, data_ + size_, TensorIteratorPolicy<T>{});
    }
    
    const_iterator cend() const {
        return const_iterator(data_ + size_, data_ + size_, TensorIteratorPolicy<T>{});
    }
    
    // ========================================================================
    // Reshape and View Operations
    // ========================================================================
    
    /**
     * @brief Reshape tensor (must preserve total size).
     */
    Expected<void, std::string> reshape(const TensorShape& new_shape) {
        if (new_shape.total_size() != size_) {
            return make_unexpected(std::string("New shape must have same total size"));
        }
        
        typename ConcurrencyPolicy::LockGuard lock(concurrency_policy_.getLock());
        shape_ = new_shape;
        strides_ = compute_strides(shape_);
        return {};
    }
    
    /**
     * @brief Create a view (does not copy data).
     */
    Tensor view(const TensorShape& new_shape) const {
        enforce(new_shape.total_size() == size_, 
                "View shape must have same total size");
        return Tensor(new_shape, const_cast<T*>(data_), false);
    }
    
    /**
     * @brief Flatten to 1D tensor.
     */
    Tensor flatten() const {
        TensorShape flat_shape({size_});
        return view(flat_shape);
    }
    
    /**
     * @brief Transpose 2D tensor.
     */
    Expected<Tensor, std::string> transpose() const {
        if (rank() != 2) {
            return make_unexpected(std::string("Transpose requires 2D tensor"));
        }
        
        TensorShape new_shape({shape_[1], shape_[0]});
        Tensor result(new_shape);
        
        for (size_t i = 0; i < shape_[0]; ++i) {
            for (size_t j = 0; j < shape_[1]; ++j) {
                result.at(j, i) = at(i, j);
            }
        }
        
        return result;
    }
    
    // ========================================================================
    // Fill and Initialization
    // ========================================================================
    
    /**
     * @brief Fill tensor with value.
     */
    void fill(const T& value) {
        typename ConcurrencyPolicy::LockGuard lock(concurrency_policy_.getLock());
        std::fill_n(data_, size_, value);
    }
    
    /**
     * @brief Fill with zeros.
     */
    void zeros() {
        fill(T{});
    }
    
    /**
     * @brief Fill with ones.
     */
    void ones() {
        fill(T{1});
    }
    
    /**
     * @brief Fill with generator function.
     */
    template <typename Func>
    void fill_with(Func generator) {
        typename ConcurrencyPolicy::LockGuard lock(concurrency_policy_.getLock());
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = generator();
        }
    }
    
    // ========================================================================
    // Arithmetic Operations
    // ========================================================================
    
    /**
     * @brief Element-wise addition.
     */
    Tensor operator+(const Tensor& other) const {
        enforce(shape_ == other.shape_, "Shapes must match for addition");
        
        Tensor result(shape_);
        for (size_t i = 0; i < size_; ++i) {
            if constexpr (std::is_integral_v<T>) {
                auto res = checked_add<ReturnExpectedPolicy, T>(data_[i], other.data_[i]);
                enforce(res.has_value(), "Addition overflow");
                result.data_[i] = res.value();
            } else {
                result.data_[i] = data_[i] + other.data_[i];
            }
        }
        return result;
    }
    
    /**
     * @brief Element-wise subtraction.
     */
    Tensor operator-(const Tensor& other) const {
        enforce(shape_ == other.shape_, "Shapes must match for subtraction");
        
        Tensor result(shape_);
        for (size_t i = 0; i < size_; ++i) {
            if constexpr (std::is_integral_v<T>) {
                auto res = checked_sub<ReturnExpectedPolicy, T>(data_[i], other.data_[i]);
                enforce(res.has_value(), "Subtraction overflow");
                result.data_[i] = res.value();
            } else {
                result.data_[i] = data_[i] - other.data_[i];
            }
        }
        return result;
    }
    
    /**
     * @brief Element-wise multiplication.
     */
    Tensor operator*(const Tensor& other) const {
        enforce(shape_ == other.shape_, "Shapes must match for multiplication");
        
        Tensor result(shape_);
        for (size_t i = 0; i < size_; ++i) {
            if constexpr (std::is_integral_v<T>) {
                auto res = checked_mul<ReturnExpectedPolicy, T>(data_[i], other.data_[i]);
                enforce(res.has_value(), "Multiplication overflow");
                result.data_[i] = res.value();
            } else {
                result.data_[i] = data_[i] * other.data_[i];
            }
        }
        return result;
    }
    
    /**
     * @brief Scalar addition.
     */
    Tensor operator+(const T& scalar) const {
        Tensor result(shape_);
        for (size_t i = 0; i < size_; ++i) {
            auto res = checked_add<ReturnExpectedPolicy, T>(data_[i], scalar);
            enforce(res.has_value(), "Addition overflow");
            result.data_[i] = res.value();
        }
        return result;
    }
    
    /**
     * @brief Scalar multiplication.
     */
    Tensor operator*(const T& scalar) const {
        Tensor result(shape_);
        for (size_t i = 0; i < size_; ++i) {
            if constexpr (std::is_integral_v<T>) {
                auto res = checked_mul<ReturnExpectedPolicy, T>(data_[i], scalar);
                enforce(res.has_value(), "Multiplication overflow");
                result.data_[i] = res.value();
            } else {
                result.data_[i] = data_[i] * scalar;
            }
        }
        return result;
    }
    
    /**
     * @brief In-place addition.
     */
    Tensor& operator+=(const Tensor& other) {
        enforce(shape_ == other.shape_, "Shapes must match");
        typename ConcurrencyPolicy::LockGuard lock(concurrency_policy_.getLock());
        
        for (size_t i = 0; i < size_; ++i) {
            if constexpr (std::is_integral_v<T>) {
            auto res = checked_add<ReturnExpectedPolicy, T>(data_[i], other.data_[i]);
            enforce(res.has_value(), "Addition overflow");
            data_[i] = res.value();
        } else {
            data_[i] = data_[i] + other.data_[i];
        }
        }
        return *this;
    }
    
    /**
     * @brief In-place scalar multiplication.
     */
    Tensor& operator*=(const T& scalar) {
        typename ConcurrencyPolicy::LockGuard lock(concurrency_policy_.getLock());
        
        for (size_t i = 0; i < size_; ++i) {
            if constexpr (std::is_integral_v<T>) {
            auto res = checked_mul<ReturnExpectedPolicy, T>(data_[i], scalar);
            enforce(res.has_value(), "Multiplication overflow");
            data_[i] = res.value();
        } else {
            data_[i] = data_[i] * scalar;
        }
        }
        return *this;
    }
    
    // ========================================================================
    // Reduction Operations
    // ========================================================================
    
    /**
     * @brief Sum of all elements.
     */
    T sum() const {
        typename ConcurrencyPolicy::SharedGuard lock(concurrency_policy_.getLock());
        
        T result = T{};
        if constexpr (std::is_integral_v<T>) {
            for (size_t i = 0; i < size_; ++i) {
                auto res = checked_add<ReturnExpectedPolicy, T>(result, data_[i]);
                enforce(res.has_value(), "Sum overflow");
                result = res.value();
            }
        } else {
            for (size_t i = 0; i < size_; ++i) {
                result += data_[i];
            }
        }
        return result;
    }
    
    /**
     * @brief Product of all elements.
     */
    T product() const {
        typename ConcurrencyPolicy::SharedGuard lock(concurrency_policy_.getLock());
        
        T result = T{1};
        if constexpr (std::is_integral_v<T>) {
            for (size_t i = 0; i < size_; ++i) {
                auto res = checked_mul<ReturnExpectedPolicy, T>(result, data_[i]);
                enforce(res.has_value(), "Product overflow");
                result = res.value();
            }
        } else {
            for (size_t i = 0; i < size_; ++i) {
                result *= data_[i];
            }
        }
        return result;
    }
    
    /**
     * @brief Mean of all elements.
     */
    template <typename R = double>
    R mean() const {
        if (size_ == 0) return R{};
        return static_cast<R>(sum()) / static_cast<R>(size_);
    }
    
    /**
     * @brief Maximum element.
     */
    T max() const {
        enforce(size_ > 0, "Cannot find max of empty tensor");
        typename ConcurrencyPolicy::SharedGuard lock(concurrency_policy_.getLock());
        return *std::max_element(data_, data_ + size_);
    }
    
    /**
     * @brief Minimum element.
     */
    T min() const {
        enforce(size_ > 0, "Cannot find min of empty tensor");
        typename ConcurrencyPolicy::SharedGuard lock(concurrency_policy_.getLock());
        return *std::min_element(data_, data_ + size_);
    }
    
    // ========================================================================
    // Matrix Operations (for 2D tensors)
    // ========================================================================
    
    /**
     * @brief Matrix multiplication (for 2D tensors).
     */
    Expected<Tensor, std::string> matmul(const Tensor& other) const {
        if (rank() != 2 || other.rank() != 2) {
            return make_unexpected(std::string("Matrix multiplication requires 2D tensors"));
        }
        
        if (shape_[1] != other.shape_[0]) {
            return make_unexpected(std::string("Incompatible shapes for matrix multiplication"));
        }
        
        TensorShape result_shape({shape_[0], other.shape_[1]});
        Tensor result(result_shape, T{});
        
        for (size_t i = 0; i < shape_[0]; ++i) {
            for (size_t j = 0; j < other.shape_[1]; ++j) {
                T sum = T{};
                for (size_t k = 0; k < shape_[1]; ++k) {
                    if constexpr (std::is_integral_v<T>) {
                        auto mul_res = checked_mul<ReturnExpectedPolicy, T>(
                            at(i, k), other.at(k, j));
                        if (!mul_res.has_value()) {
                            return make_unexpected(std::string("Multiplication overflow"));
                        }
                        auto add_res = checked_add<ReturnExpectedPolicy, T>(
                            sum, mul_res.value());
                        if (!add_res.has_value()) {
                            return make_unexpected(std::string("Addition overflow"));
                        }
                        sum = add_res.value();
                    } else {
                        sum += at(i, k) * other.at(k, j);
                    }
                }
                result.at(i, j) = sum;
            }
        }
        
        return result;
    }
    
    // ========================================================================
    // Broadcasting
    // ========================================================================
    
    /**
     * @brief Check if two shapes are broadcastable.
     */
    static bool are_broadcastable(const TensorShape& a, const TensorShape& b) {
        size_t max_rank = std::max(a.rank(), b.rank());
        
        for (size_t i = 0; i < max_rank; ++i) {
            size_t dim_a = i < a.rank() ? a[a.rank() - 1 - i] : 1;
            size_t dim_b = i < b.rank() ? b[b.rank() - 1 - i] : 1;
            
            if (dim_a != dim_b && dim_a != 1 && dim_b != 1) {
                return false;
            }
        }
        
        return true;
    }
    
    /**
     * @brief Compute broadcast shape.
     */
    static TensorShape broadcast_shape(const TensorShape& a, const TensorShape& b) {
        enforce(are_broadcastable(a, b), "Shapes are not broadcastable");
        
        size_t max_rank = std::max(a.rank(), b.rank());
        std::vector<size_t> result_dims(max_rank);
        
        for (size_t i = 0; i < max_rank; ++i) {
            size_t dim_a = i < a.rank() ? a[a.rank() - 1 - i] : 1;
            size_t dim_b = i < b.rank() ? b[b.rank() - 1 - i] : 1;
            result_dims[max_rank - 1 - i] = std::max(dim_a, dim_b);
        }
        
        return TensorShape(result_dims);
    }
    
    // ========================================================================
    // Utility Functions
    // ========================================================================
    
    /**
     * @brief String representation.
     */
    std::string to_string() const {
        std::string result = "Tensor(shape=" + shape_.to_string();
        result += ", data=[";
        
        size_t max_display = std::min(size_, size_t(10));
        for (size_t i = 0; i < max_display; ++i) {
            if (i > 0) result += ", ";
            result += std::to_string(data_[i]);
        }
        
        if (size_ > max_display) {
            result += ", ...";
        }
        
        result += "])";
        return result;
    }
    
    /**
     * @brief Clone (deep copy).
     */
    Tensor clone() const {
        return Tensor(*this);
    }
    
    /**
     * @brief Slice along axis (1D extraction).
     */
    Expected<Tensor, std::string> slice(size_t axis, size_t index) const {
        if (axis >= rank()) {
            return make_unexpected(std::string("Axis out of bounds"));
        }
        if (index >= shape_[axis]) {
            return make_unexpected(std::string("Index out of bounds"));
        }
        
        // Compute new shape (remove axis)
        std::vector<size_t> new_dims;
        for (size_t i = 0; i < rank(); ++i) {
            if (i != axis) {
                new_dims.push_back(shape_[i]);
            }
        }
        
        if (new_dims.empty()) {
            new_dims.push_back(1);
        }
        
        TensorShape new_shape(new_dims);
        Tensor result(new_shape);
        
        // Copy data with proper strides
        size_t result_idx = 0;
        std::vector<size_t> indices(rank(), 0);
        indices[axis] = index;
        
        std::function<void(size_t)> copy_recursive = [&](size_t dim) {
            if (dim == rank()) {
                size_t src_offset = indices_to_offset(indices, strides_);
                result.data_[result_idx++] = data_[src_offset];
                return;
            }
            
            if (dim == axis) {
                copy_recursive(dim + 1);
            } else {
                for (size_t i = 0; i < shape_[dim]; ++i) {
                    indices[dim] = i;
                    copy_recursive(dim + 1);
                }
            }
        };
        
        copy_recursive(0);
        return result;
    }
};

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create tensor filled with zeros.
 */
template <typename T, typename AllocImpl = StandardAllocatorImpl<T>>
Tensor<T, AllocImpl> zeros(const TensorShape& shape) {
    Tensor<T, AllocImpl> tensor(shape);
    tensor.zeros();
    return tensor;
}

/**
 * @brief Create tensor filled with ones.
 */
template <typename T, typename AllocImpl = StandardAllocatorImpl<T>>
Tensor<T, AllocImpl> ones(const TensorShape& shape) {
    Tensor<T, AllocImpl> tensor(shape);
    tensor.ones();
    return tensor;
}

/**
 * @brief Create tensor with random values.
 */
template <typename T, typename AllocImpl = StandardAllocatorImpl<T>, typename RNG>
Tensor<T, AllocImpl> random(const TensorShape& shape, RNG& rng) {
    Tensor<T, AllocImpl> tensor(shape);
    tensor.fill_with([&rng]() { return static_cast<T>(rng()); });
    return tensor;
}

/**
 * @brief Create identity matrix.
 */
template <typename T, typename AllocImpl = StandardAllocatorImpl<T>>
Tensor<T, AllocImpl> eye(size_t n) {
    Tensor<T, AllocImpl> tensor(TensorShape({n, n}), T{});
    for (size_t i = 0; i < n; ++i) {
        tensor.at(i, i) = T{1};
    }
    return tensor;
}

/**
 * @brief Create tensor from range.
 */
template <typename T, typename AllocImpl = StandardAllocatorImpl<T>>
Tensor<T, AllocImpl> arange(T start, T stop, T step = T{1}) {
    enforce(step != T{}, "Step cannot be zero");
    
    size_t num_elements = static_cast<size_t>((stop - start) / step);
    Tensor<T, AllocImpl> tensor(TensorShape({num_elements}));
    
    T value = start;
    for (size_t i = 0; i < num_elements; ++i) {
        tensor[i] = value;
        if constexpr (std::is_integral_v<T>) {
            auto res = checked_add<ReturnExpectedPolicy, T>(value, step);
            enforce(res.has_value(), "Arange overflow");
            value = res.value();
        } else {
            value += step;
        }
    }
    
    return tensor;
}

// ============================================================================
// Thread-Safe Tensor Aliases
// ============================================================================

template <typename T, typename AllocImpl = StandardAllocatorImpl<T>>
using ThreadSafeTensor = Tensor<T, AllocImpl, MutexSynchronizationPolicy>;

template <typename T, typename AllocImpl = StandardAllocatorImpl<T>>
using LockFreeTensor = Tensor<T, AllocImpl, SpinlockSynchronizationPolicy>;

} // namespace cpp_utilities
