// tf/tensor.hpp
// RAII wrapper for TF_Tensor with thread-safe data access
//
// PATCHED v4 - Fixes from ChatGPT review comparison:
// - P0-C: Shared state (TensorState) so views cannot outlive tensor data
// - P0-D: Call deallocator on TF_NewTensor failure (prevents memory leak)
// - P1: Removed dangerous default deallocator; explicit Adopt API
//
// Original merged implementation credits:
// - ChatGPT: TF_TensorElementCount, ensure_tensor_/ensure_dtype_ helpers
// - Claude: 11 scalar types, detailed error messages, Allocate factory

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <concepts>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <source_location>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

extern "C" {
#include <tensorflow/c/c_api.h>
}

#include "tf/format.hpp"

#include "tf/guarded_span.hpp"
#include "tf/policy.hpp"
#include "tf/status.hpp"

namespace tf {

// ============================================================================
// Helpers
// ============================================================================

namespace detail {
    /// Helper for static_assert in constexpr if
    template<class>
    inline constexpr bool always_false_v = false;
    
    /// Checked multiplication for size calculations - throws on overflow
    /// C++20 doesn't have std::checked_*, so we implement manually
    [[nodiscard]] inline std::size_t checked_mul(std::size_t a, std::size_t b,
                                                  const char* context = "size calculation") {
        if (a == 0 || b == 0) return 0;
        
        // Check: a * b > MAX  <==>  b > MAX / a
        if (b > std::numeric_limits<std::size_t>::max() / a) {
            throw std::overflow_error(tf::detail::format(
                "Integer overflow in {}: {} * {} exceeds size_t max",
                context, a, b));
        }
        return a * b;
    }
    
    /// Compute product of dimensions with overflow checking
    [[nodiscard]] inline std::size_t checked_product(std::span<const std::int64_t> dims,
                                                      const char* context = "shape calculation") {
        std::size_t result = 1;
        for (auto d : dims) {
            if (d < 0) {
                throw std::invalid_argument(tf::detail::format(
                    "Negative dimension {} in {}", d, context));
            }
            result = checked_mul(result, static_cast<std::size_t>(d), context);
        }
        return result;
    }
}

// ============================================================================
// TensorScalar Concept - All supported element types
// ============================================================================

template<class T>
concept TensorScalar =
    std::same_as<T, float>         ||
    std::same_as<T, double>        ||
    std::same_as<T, std::int8_t>   ||
    std::same_as<T, std::int16_t>  ||
    std::same_as<T, std::int32_t>  ||
    std::same_as<T, std::int64_t>  ||
    std::same_as<T, std::uint8_t>  ||
    std::same_as<T, std::uint16_t> ||
    std::same_as<T, std::uint32_t> ||
    std::same_as<T, std::uint64_t> ||
    std::same_as<T, bool>;

// ============================================================================
// Type mapping: C++ type -> TF_DataType
// ============================================================================

/// Get TF_DataType for a C++ scalar type
template<TensorScalar T>
[[nodiscard]] constexpr TF_DataType tf_dtype_of() noexcept {
    if constexpr (std::same_as<T, float>)              return TF_FLOAT;
    else if constexpr (std::same_as<T, double>)        return TF_DOUBLE;
    else if constexpr (std::same_as<T, std::int8_t>)   return TF_INT8;
    else if constexpr (std::same_as<T, std::int16_t>)  return TF_INT16;
    else if constexpr (std::same_as<T, std::int32_t>)  return TF_INT32;
    else if constexpr (std::same_as<T, std::int64_t>)  return TF_INT64;
    else if constexpr (std::same_as<T, std::uint8_t>)  return TF_UINT8;
    else if constexpr (std::same_as<T, std::uint16_t>) return TF_UINT16;
    else if constexpr (std::same_as<T, std::uint32_t>) return TF_UINT32;
    else if constexpr (std::same_as<T, std::uint64_t>) return TF_UINT64;
    else if constexpr (std::same_as<T, bool>)          return TF_BOOL;
    else static_assert(detail::always_false_v<T>, "Unsupported scalar type");
}

/// Variable template for convenience
template<TensorScalar T>
inline constexpr TF_DataType tf_dtype_v = tf_dtype_of<T>();

/// Get human-readable name for TF_DataType
[[nodiscard]] constexpr const char* dtype_name(TF_DataType dtype) noexcept {
    switch (dtype) {
        case TF_FLOAT:      return "float32";
        case TF_DOUBLE:     return "float64";
        case TF_INT8:       return "int8";
        case TF_INT16:      return "int16";
        case TF_INT32:      return "int32";
        case TF_INT64:      return "int64";
        case TF_UINT8:      return "uint8";
        case TF_UINT16:     return "uint16";
        case TF_UINT32:     return "uint32";
        case TF_UINT64:     return "uint64";
        case TF_BOOL:       return "bool";
        case TF_STRING:     return "string";
        case TF_COMPLEX64:  return "complex64";
        case TF_COMPLEX128: return "complex128";
        case TF_BFLOAT16:   return "bfloat16";
        case TF_HALF:       return "float16";
        default:            return "unknown";
    }
}

// ============================================================================
// TensorState - Shared internal state (P0-C FIX: enables safe view lifetime)
// ============================================================================
// Views hold a shared_ptr to this state, keeping the TF_Tensor and mutex
// alive even if the original Tensor object is destroyed.

template<class Policy>
struct TensorState {
    TF_Tensor* tensor{nullptr};
    std::vector<std::int64_t> shape;
    mutable Policy policy;
    
    TensorState() = default;
    
    TensorState(TF_Tensor* t, std::vector<std::int64_t> s)
        : tensor(t), shape(std::move(s)) {}
    
    ~TensorState() {
        if (tensor) TF_DeleteTensor(tensor);
    }
    
    // Non-copyable (shared via shared_ptr)
    TensorState(const TensorState&) = delete;
    TensorState& operator=(const TensorState&) = delete;
};

// ============================================================================
// TensorReadView / TensorWriteView - Views that keep tensor alive (P0-C FIX)
// ============================================================================
// These views hold both:
// 1. A shared_ptr to TensorState (keeps TF_Tensor and mutex alive)
// 2. A lock guard (provides thread-safety)
// This prevents the dangling view bug where returning a view from a scope
// where the Tensor is destroyed leads to use-after-free.

template<class T, class Policy, class Guard>
    requires TensorScalar<std::remove_cv_t<T>>
class TensorView {
public:
    // STL-compatible type aliases
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /// Construct view (called by Tensor::read/write)
    TensorView(std::shared_ptr<TensorState<Policy>> state,
               std::span<T> data,
               Guard guard) noexcept(std::is_nothrow_move_constructible_v<Guard>)
        : state_(std::move(state))
        , span_(data)
        , guard_(std::move(guard))
    {}

    // Non-copyable (can't duplicate the lock)
    TensorView(const TensorView&) = delete;
    TensorView& operator=(const TensorView&) = delete;

    // Movable (transfers ownership)
    TensorView(TensorView&&) noexcept = default;
    TensorView& operator=(TensorView&&) noexcept = default;

    ~TensorView() = default;

    // ─────────────────────────────────────────────────────────────────
    // Element access
    // ─────────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr reference operator[](size_type i) const noexcept {
        return span_[i];
    }

    [[nodiscard]] constexpr reference at(size_type i) const {
        if (i >= size()) {
            throw std::out_of_range("TensorView::at: index out of range");
        }
        return span_[i];
    }

    [[nodiscard]] constexpr reference front() const noexcept { return span_.front(); }
    [[nodiscard]] constexpr reference back() const noexcept { return span_.back(); }
    [[nodiscard]] constexpr pointer data() const noexcept { return span_.data(); }

    // ─────────────────────────────────────────────────────────────────
    // Span access
    // ─────────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr std::span<T> span() const noexcept { return span_; }
    [[nodiscard]] constexpr operator std::span<T>() const noexcept { return span_; }

    // ─────────────────────────────────────────────────────────────────
    // Iterators
    // ─────────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr iterator begin() const noexcept { return span_.data(); }
    [[nodiscard]] constexpr iterator end() const noexcept { return span_.data() + span_.size(); }
    [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }
    [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

    [[nodiscard]] constexpr reverse_iterator rbegin() const noexcept {
        return reverse_iterator(end());
    }
    [[nodiscard]] constexpr reverse_iterator rend() const noexcept {
        return reverse_iterator(begin());
    }
    [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept {
        return const_reverse_iterator(cend());
    }
    [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept {
        return const_reverse_iterator(cbegin());
    }

    // ─────────────────────────────────────────────────────────────────
    // Capacity
    // ─────────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr size_type size() const noexcept { return span_.size(); }
    [[nodiscard]] constexpr size_type size_bytes() const noexcept { return span_.size_bytes(); }
    [[nodiscard]] constexpr bool empty() const noexcept { return span_.empty(); }

    // ─────────────────────────────────────────────────────────────────
    // Subviews - INTENTIONALLY OMITTED
    // ─────────────────────────────────────────────────────────────────
    // Methods like first(), last(), subspan() that return raw std::span
    // are not provided because they create lifetime hazards:
    //
    //   std::span<float> bad;
    //   { 
    //       auto view = tensor.read<float>();
    //       bad = view.first(10);  // Raw span escapes!
    //   }  // view destroyed, lock released
    //   // bad now dangles - UB if accessed
    //
    // Safe alternatives:
    //   - Use operator[] or at() for element access
    //   - Use begin()/end() for iteration
    //   - Use ToVector<T>() to extract a copy
    //   - Keep the view alive for the duration of use
    // ─────────────────────────────────────────────────────────────────

private:
    std::shared_ptr<TensorState<Policy>> state_;  // Keeps tensor+mutex alive
    std::span<T> span_;
    [[no_unique_address]] Guard guard_;
};

// ============================================================================
// Tensor - RAII wrapper for TF_Tensor with thread-safe access
// ============================================================================

template<class Policy = policy::NoLock>
    requires policy::LockPolicy<Policy>
class Tensor {
public:
    // ─────────────────────────────────────────────────────────────────
    // Type aliases
    // ─────────────────────────────────────────────────────────────────

    using policy_type = Policy;
    using shared_guard_type = decltype(std::declval<const Policy&>().scoped_shared());
    using exclusive_guard_type = decltype(std::declval<const Policy&>().scoped_lock());

    // P0-C FIX: Views now hold shared_ptr to state, preventing dangling
    template<TensorScalar T>
    using ReadView = TensorView<const T, Policy, shared_guard_type>;

    template<TensorScalar T>
    using WriteView = TensorView<T, Policy, exclusive_guard_type>;

    // ─────────────────────────────────────────────────────────────────
    // Constructors
    // ─────────────────────────────────────────────────────────────────

    /// Default constructor (empty tensor)
    Tensor() : state_(std::make_shared<TensorState<Policy>>()) {}

    /// Destructor (shared_ptr handles cleanup)
    ~Tensor() = default;

    // ─────────────────────────────────────────────────────────────────
    // Move semantics (non-copyable)
    // Moved-from Tensor is left in valid empty state (like std::vector)
    // Note: Not noexcept because we create new state for moved-from object
    // ─────────────────────────────────────────────────────────────────

    Tensor(const Tensor&) = delete;
    Tensor& operator=(const Tensor&) = delete;

    Tensor(Tensor&& other)
        : state_(std::move(other.state_))
    {
        // Leave other in valid empty state
        other.state_ = std::make_shared<TensorState<Policy>>();
    }
    
    Tensor& operator=(Tensor&& other) {
        if (this != &other) {
            state_ = std::move(other.state_);
            // Leave other in valid empty state
            other.state_ = std::make_shared<TensorState<Policy>>();
        }
        return *this;
    }

    // ─────────────────────────────────────────────────────────────────
    // Queries (no locking - immutable after construction)
    // ─────────────────────────────────────────────────────────────────

    /// Get shape as vector of dimensions
    [[nodiscard]] const std::vector<std::int64_t>& shape() const noexcept {
        return state_->shape;
    }

    /// Get number of dimensions (rank)
    [[nodiscard]] int rank() const noexcept {
        return static_cast<int>(state_->shape.size());
    }

    /// Get data type
    [[nodiscard]] TF_DataType dtype() const noexcept {
        return state_->tensor ? TF_TensorType(state_->tensor) : TF_FLOAT;
    }

    /// Get human-readable data type name
    [[nodiscard]] const char* dtype_name() const noexcept {
        return tf::dtype_name(dtype());
    }

    /// Get total byte size
    [[nodiscard]] std::size_t byte_size() const noexcept {
        return state_->tensor ? TF_TensorByteSize(state_->tensor) : 0;
    }

    /// Get number of elements (uses TF API directly)
    [[nodiscard]] std::size_t num_elements() const {
        if (!state_->tensor) return 0;
        const std::int64_t n = TF_TensorElementCount(state_->tensor);
        if (n < 0) {
            throw std::runtime_error("TF_TensorElementCount returned negative value");
        }
        return static_cast<std::size_t>(n);
    }

    /// Check if tensor is empty/null
    [[nodiscard]] bool empty() const noexcept {
        return state_->tensor == nullptr;
    }

    /// Explicit bool conversion
    [[nodiscard]] explicit operator bool() const noexcept {
        return state_->tensor != nullptr;
    }

    /// Get raw TF_Tensor handle
    [[nodiscard]] TF_Tensor* handle() const noexcept {
        return state_->tensor;
    }

    // ─────────────────────────────────────────────────────────────────
    // Data extraction (copies data out of tensor)
    // ─────────────────────────────────────────────────────────────────

    /// Extract tensor data as a vector (copies data)
    /// @throws std::runtime_error if tensor is empty
    /// @throws std::runtime_error if type mismatch
    template<TensorScalar T>
    [[nodiscard]] std::vector<T> ToVector() const {
        ensure_tensor_("ToVector");
        ensure_dtype_<T>("ToVector");
        
        auto view = read<T>();
        return std::vector<T>(view.begin(), view.end());
    }

    /// Extract single scalar value
    /// @throws std::runtime_error if tensor is empty or has more than 1 element
    /// @throws std::runtime_error if type mismatch
    template<TensorScalar T>
    [[nodiscard]] T ToScalar() const {
        ensure_tensor_("ToScalar");
        ensure_dtype_<T>("ToScalar");
        
        if (num_elements() != 1) {
            throw std::runtime_error(tf::detail::format(
                "Tensor::ToScalar(): expected 1 element, got {}", num_elements()));
        }
        
        auto view = read<T>();
        return view[0];
    }

    /// Deep copy this tensor (returns new tensor with copied data)
    /// @returns A new Tensor with identical dtype, shape, and data
    /// @note Returns empty tensor if this tensor is empty
    [[nodiscard]] Tensor Clone() const {
        if (!state_->tensor) {
            return Tensor{};  // Clone of empty is empty
        }
        
        const auto dt = dtype();
        const auto& sh = shape();
        const auto bytes = byte_size();
        
        return create_tensor_alloc_(
            dt,
            sh,
            bytes,
            [this](void* dst, std::size_t len) {
                if (len != 0) {
                    std::memcpy(dst, TF_TensorData(state_->tensor), len);
                }
            });
    }

    // ─────────────────────────────────────────────────────────────────
    // THREAD-SAFE DATA ACCESS (guarded views with lifetime safety)
    // P0-C FIX: Views hold shared_ptr to state, so they can safely
    // outlive the Tensor object without dangling pointers.
    // ─────────────────────────────────────────────────────────────────

    /// Read access - returns view holding shared lock AND state reference
    template<TensorScalar T>
    [[nodiscard]] ReadView<T> read() const {
        ensure_tensor_("read");
        auto guard = state_->policy.scoped_shared();
        ensure_dtype_<T>("read");
        
        const auto n = num_elements();
        const T* ptr = static_cast<const T*>(TF_TensorData(state_->tensor));
        return ReadView<T>(state_, std::span<const T>(ptr, n), std::move(guard));
    }

    /// Write access - returns view holding exclusive lock AND state reference
    template<TensorScalar T>
    [[nodiscard]] WriteView<T> write() {
        ensure_tensor_("write");
        auto guard = state_->policy.scoped_lock();
        ensure_dtype_<T>("write");
        
        const auto n = num_elements();
        T* ptr = static_cast<T*>(TF_TensorData(state_->tensor));
        return WriteView<T>(state_, std::span<T>(ptr, n), std::move(guard));
    }

    /// Callback-based read access (still valid, but views are now safe too)
    template<TensorScalar T, class Fn>
    decltype(auto) with_read(Fn&& fn) const {
        auto view = read<T>();
        return std::forward<Fn>(fn)(view.span());
    }

    /// Callback-based write access
    template<TensorScalar T, class Fn>
    decltype(auto) with_write(Fn&& fn) {
        auto view = write<T>();
        return std::forward<Fn>(fn)(view.span());
    }

    // ─────────────────────────────────────────────────────────────────
    // UNSAFE DATA ACCESS (advanced users only)
    // WARNING: No lock is held! Caller must synchronize externally.
    // ─────────────────────────────────────────────────────────────────

    template<TensorScalar T>
    [[nodiscard]] T* unsafe_data() {
        ensure_tensor_("unsafe_data");
        ensure_dtype_<T>("unsafe_data");
        return static_cast<T*>(TF_TensorData(state_->tensor));
    }

    template<TensorScalar T>
    [[nodiscard]] const T* unsafe_data() const {
        ensure_tensor_("unsafe_data");
        ensure_dtype_<T>("unsafe_data");
        return static_cast<const T*>(TF_TensorData(state_->tensor));
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Lock acquisition (for advanced patterns like Session feed locking)
    // ─────────────────────────────────────────────────────────────────
    
    /// Acquire a shared (read) lock without accessing data.
    /// Useful for holding locks on feed tensors during Session::Run().
    /// 
    /// Example:
    ///   auto guard = tensor.acquire_shared_lock();
    ///   session.Run({Feed{"input", tensor}}, ...);
    ///   // guard keeps tensor locked until scope exit
    [[nodiscard]] shared_guard_type acquire_shared_lock() const {
        return state_->policy.scoped_shared();
    }
    
    /// Acquire an exclusive (write) lock without accessing data.
    [[nodiscard]] exclusive_guard_type acquire_exclusive_lock() const {
        return state_->policy.scoped_lock();
    }

    // ─────────────────────────────────────────────────────────────────
    // Factory: FromVector (copies data) - SAFE
    // ─────────────────────────────────────────────────────────────────

    template<TensorScalar T>
    [[nodiscard]] static Tensor FromVector(
        std::span<const std::int64_t> dims,
        const std::vector<T>& values,
        std::source_location loc = std::source_location::current())
    {
        const std::size_t expected = detail::checked_product(dims, "Tensor::FromVector");

        if (expected != values.size()) {
            throw std::invalid_argument(tf::detail::format(
                "Tensor::FromVector at {}:{}: shape requires {} elements, got {}",
                loc.file_name(), loc.line(), expected, values.size()));
        }

        const std::size_t bytes = detail::checked_mul(expected, sizeof(T), "Tensor::FromVector");
        return create_tensor_alloc_(
            tf_dtype_v<T>,
            dims,
            bytes,
            [&](void* dst, std::size_t len) {
                if (len != 0) {
                    std::memcpy(dst, values.data(), len);
                }
            });
    }

    /// FromVector overload accepting initializer_list for shape
    template<TensorScalar T>
    [[nodiscard]] static Tensor FromVector(
        std::initializer_list<std::int64_t> dims,
        std::initializer_list<T> values,
        std::source_location loc = std::source_location::current())
    {
        std::vector<T> vals(values);
        std::vector<std::int64_t> shape_vec(dims);
        return FromVector<T>(shape_vec, vals, loc);
    }

    // ─────────────────────────────────────────────────────────────────
    // Factory: FromScalar - Creates true TensorFlow scalar (shape [])
    // ─────────────────────────────────────────────────────────────────

    template<TensorScalar T>
    [[nodiscard]] static Tensor FromScalar(T value) {
        const std::size_t bytes = sizeof(T);
        // Empty span = scalar (rank 0, shape [])
        return create_tensor_alloc_(
            tf_dtype_v<T>,
            std::span<const std::int64_t>{},
            bytes,
            [&](void* dst, std::size_t len) {
                if (len != 0) {
                    std::memcpy(dst, &value, len);
                }
            });
    }

    // ─────────────────────────────────────────────────────────────────
    // Factory: FromRaw (adopts ownership) - Use with caution
    // ─────────────────────────────────────────────────────────────────

    [[nodiscard]] static Tensor FromRaw(TF_Tensor* raw) {
        if (!raw) {
            throw std::invalid_argument("Tensor::FromRaw: null TF_Tensor*");
        }

        Tensor t;
        t.state_->tensor = raw;  // Adopt ownership

        // Extract shape using CORRECT TF C API
        const int ndims = TF_NumDims(raw);
        t.state_->shape.reserve(static_cast<std::size_t>(ndims));
        for (int i = 0; i < ndims; ++i) {
            t.state_->shape.push_back(TF_Dim(raw, i));
        }

        return t;
    }

    // ─────────────────────────────────────────────────────────────────
    // Factory: Allocate (uninitialized memory) - SAFE
    // ─────────────────────────────────────────────────────────────────

    template<TensorScalar T>
    [[nodiscard]] static Tensor Allocate(std::span<const std::int64_t> dims) {
        const std::size_t num_elems = detail::checked_product(dims, "Tensor::Allocate");
        const std::size_t bytes = detail::checked_mul(num_elems, sizeof(T), "Tensor::Allocate");
        return create_tensor_alloc_(
            tf_dtype_v<T>,
            dims,
            bytes,
            [](void*, std::size_t) {});
    }

    /// Allocate overload accepting initializer_list for shape
    template<TensorScalar T>
    [[nodiscard]] static Tensor Allocate(std::initializer_list<std::int64_t> dims) {
        std::vector<std::int64_t> shape_vec(dims);
        return Allocate<T>(shape_vec);
    }

    // ─────────────────────────────────────────────────────────────────
    // Factory: Zeros (zero-initialized) - SAFE
    // ─────────────────────────────────────────────────────────────────

    template<TensorScalar T>
    [[nodiscard]] static Tensor Zeros(std::span<const std::int64_t> dims) {
        const std::size_t num_elems = detail::checked_product(dims, "Tensor::Zeros");
        const std::size_t bytes = detail::checked_mul(num_elems, sizeof(T), "Tensor::Zeros");
        return create_tensor_alloc_(
            tf_dtype_v<T>,
            dims,
            bytes,
            [](void* dst, std::size_t len) {
                if (len != 0) {
                    std::memset(dst, 0, len);
                }
            });
    }

    /// Zeros overload accepting initializer_list for shape
    template<TensorScalar T>
    [[nodiscard]] static Tensor Zeros(std::initializer_list<std::int64_t> dims) {
        std::vector<std::int64_t> shape_vec(dims);
        return Zeros<T>(shape_vec);
    }

    // ─────────────────────────────────────────────────────────────────
    // Factory: AdoptMalloc - Adopt malloc'd memory (P1 FIX: explicit API)
    // Use this when you have malloc'd memory to adopt.
    // ─────────────────────────────────────────────────────────────────

    template<TensorScalar T>
    [[nodiscard]] static Tensor AdoptMalloc(
        std::span<const std::int64_t> dims,
        void* data,
        std::size_t byte_len)
    {
        if (!data && byte_len > 0) {
            throw std::invalid_argument("Tensor::AdoptMalloc: null data with non-zero byte_len");
        }

        const std::size_t num_elems = detail::checked_product(dims, "Tensor::AdoptMalloc");
        const std::size_t expected_bytes = detail::checked_mul(
            num_elems, sizeof(T), "Tensor::AdoptMalloc");

        if (expected_bytes != byte_len) {
            throw std::invalid_argument(tf::detail::format(
                "Tensor::AdoptMalloc: byte_len mismatch - expected {} but got {}",
                expected_bytes, byte_len));
        }

        return create_tensor_adopt_(
            tf_dtype_v<T>, dims, data, byte_len, &default_deallocator, nullptr);
    }

    // ─────────────────────────────────────────────────────────────────
    // Factory: Adopt - Adopt memory with explicit deallocator (P1 FIX)
    // NO DEFAULT DEALLOCATOR - caller must provide one.
    // ─────────────────────────────────────────────────────────────────

    using Deallocator = void (*)(void* data, std::size_t len, void* arg);

    [[nodiscard]] static Tensor Adopt(
        TF_DataType dtype,
        std::span<const std::int64_t> dims,
        void* data,
        std::size_t byte_len,
        Deallocator deallocator,  // REQUIRED - no default!
        void* deallocator_arg = nullptr)
    {
        if (!deallocator) {
            throw std::invalid_argument(
                "Tensor::Adopt: deallocator is required (use AdoptMalloc for malloc'd data)");
        }
        if (!data && byte_len > 0) {
            throw std::invalid_argument("Tensor::Adopt: null data with non-zero byte_len");
        }

        const std::size_t dtype_size = TF_DataTypeSize(dtype);
        if (dtype_size == 0) {
            throw std::invalid_argument(
                "Tensor::Adopt: variable-length dtype (TF_DataTypeSize==0) is not supported");
        }

        const std::size_t num_elems = detail::checked_product(dims, "Tensor::Adopt");
        const std::size_t expected_bytes = detail::checked_mul(
            num_elems, dtype_size, "Tensor::Adopt");

        if (expected_bytes != byte_len) {
            throw std::invalid_argument(tf::detail::format(
                "Tensor::Adopt: byte_len mismatch - expected {} but got {}",
                expected_bytes, byte_len));
        }

        return create_tensor_adopt_(dtype, dims, data, byte_len, deallocator, deallocator_arg);
    }

private:
    std::shared_ptr<TensorState<Policy>> state_;

    // ─────────────────────────────────────────────────────────────────
    // Private: Central tensor creation helpers
    // ─────────────────────────────────────────────────────────────────

    // Create a tensor whose backing store is allocated by TensorFlow.
    // This avoids requiring std::malloc/free and matches TensorFlow's
    // preferred alignment guarantees (see TF_AllocateTensor docs).
    template<class InitFn>
    [[nodiscard]] static Tensor create_tensor_alloc_(
        TF_DataType dtype,
        std::span<const std::int64_t> dims,
        std::size_t byte_len,
        InitFn&& init)
    {
        Tensor t;  // Ensure shared state is created before any raw TF allocation
        std::vector<std::int64_t> shape_vec(dims.begin(), dims.end());

        const int64_t* dims_ptr = shape_vec.empty() ? nullptr : shape_vec.data();
        const int num_dims = static_cast<int>(shape_vec.size());

        TF_Tensor* tensor = TF_AllocateTensor(dtype, dims_ptr, num_dims, byte_len);
        if (!tensor) {
            throw std::runtime_error("TF_AllocateTensor: failed to create tensor");
        }

        if (byte_len != 0) {
            void* dst = TF_TensorData(tensor);
            try {
                init(dst, byte_len);
            } catch (...) {
                TF_DeleteTensor(tensor);
                throw;
            }
        }

        t.state_->tensor = tensor;
        t.state_->shape = std::move(shape_vec);
        return t;
    }

    // Create a tensor that adopts caller-provided memory.
    // Uses DataGuard RAII to prevent memory leaks on exception.
    [[nodiscard]] static Tensor create_tensor_adopt_(
        TF_DataType dtype,
        std::span<const std::int64_t> dims,
        void* data,
        std::size_t byte_len,
        Deallocator deallocator,
        void* deallocator_arg)
    {
        // RAII guard ensures data is cleaned up if we throw before TF_NewTensor succeeds
        struct DataGuard {
            void* data;
            std::size_t len;
            Deallocator dealloc;
            void* arg;
            bool released{false};
            
            ~DataGuard() {
                if (!released && dealloc && data) {
                    dealloc(data, len, arg);
                }
            }
            
            void release() noexcept { released = true; }
        };
        
        DataGuard guard{data, byte_len, deallocator, deallocator_arg};
        
        Tensor t;  // Can throw bad_alloc - guard will clean up
        std::vector<std::int64_t> shape_vec(dims.begin(), dims.end());  // Can throw - guard will clean up

        const int64_t* dims_ptr = shape_vec.empty() ? nullptr : shape_vec.data();
        const int num_dims = static_cast<int>(shape_vec.size());

        TF_Tensor* tensor = TF_NewTensor(
            dtype,
            dims_ptr,
            num_dims,
            data,
            byte_len,
            deallocator,
            deallocator_arg);

        if (!tensor) {
            // Guard will clean up data when we throw
            throw std::runtime_error("TF_NewTensor: failed to create tensor");
        }

        // Success - TF_NewTensor took ownership, don't deallocate
        guard.release();

        t.state_->tensor = tensor;
        t.state_->shape = std::move(shape_vec);
        return t;
    }

    // ─────────────────────────────────────────────────────────────────
    // Private helpers
    // ─────────────────────────────────────────────────────────────────

    void ensure_tensor_(const char* fn) const {
        if (!state_->tensor) {
            throw std::runtime_error(tf::detail::format(
                "Tensor::{}(): tensor is null/empty", fn));
        }
    }

    template<TensorScalar T>
    void ensure_dtype_(const char* fn) const {
        const TF_DataType expected = tf_dtype_v<T>;
        const TF_DataType actual = dtype();
        if (actual != expected) {
            throw std::runtime_error(tf::detail::format(
                "Tensor::{}(): dtype mismatch - requested {} but tensor is {}",
                fn, tf::dtype_name(expected), tf::dtype_name(actual)));
        }
    }

    static void default_deallocator(void* data, std::size_t, void*) noexcept {
        std::free(data);
    }
};

// ============================================================================
// Type aliases
// ============================================================================

using FastTensor = Tensor<policy::NoLock>;
using SafeTensor = Tensor<policy::Mutex>;
using SharedTensor = Tensor<policy::SharedMutex>;

} // namespace tf
