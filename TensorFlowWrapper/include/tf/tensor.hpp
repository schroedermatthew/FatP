// tf/tensor.hpp
// RAII wrapper for TF_Tensor with thread-safe data access
//
// MERGED IMPLEMENTATION - Best of ChatGPT + Claude:
// - ChatGPT: TF_TensorElementCount, ensure_tensor_/ensure_dtype_ helpers,
//            explicit requires clause on class
// - Claude: 11 scalar types, detailed error messages, Allocate factory
//
// Fixes applied:
// - P0: FromRaw uses TF_NumDims/TF_Dim (TF_TensorDims doesn't exist!)
// - P0: No invalid requires-clause on FromRaw (runtime check instead)
// - P0: always_false_v defined
// - P1: No noexcept on throwing functions
// - P1: Guarded views hold lock for lifetime
// - P3: [[nodiscard]] on all factories

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <concepts>
#include <format>
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

    template<TensorScalar T>
    using ReadView = GuardedSpan<const T, shared_guard_type>;

    template<TensorScalar T>
    using WriteView = GuardedSpan<T, exclusive_guard_type>;

    // ─────────────────────────────────────────────────────────────────
    // Constructors
    // ─────────────────────────────────────────────────────────────────

    /// Default constructor (empty tensor)
    Tensor() noexcept = default;

    /// Construct from raw data (takes ownership via deallocator)
    Tensor(TF_DataType dtype,
           std::span<const std::int64_t> dims,
           void* data,
           std::size_t byte_len,
           void (*deallocator)(void*, std::size_t, void*) = nullptr,
           void* deallocator_arg = nullptr)
    {
        if (!deallocator) deallocator = &default_deallocator;

        shape_.assign(dims.begin(), dims.end());

        tensor_ = TF_NewTensor(
            dtype,
            shape_.data(),
            static_cast<int>(shape_.size()),
            data,
            byte_len,
            deallocator,
            deallocator_arg);

        if (!tensor_) {
            throw std::runtime_error("TF_NewTensor: failed to create tensor");
        }
    }

    /// Destructor
    ~Tensor() {
        if (tensor_) TF_DeleteTensor(tensor_);
    }

    // ─────────────────────────────────────────────────────────────────
    // Move semantics (non-copyable)
    // ─────────────────────────────────────────────────────────────────

    Tensor(const Tensor&) = delete;
    Tensor& operator=(const Tensor&) = delete;

    Tensor(Tensor&& other) noexcept
        : tensor_(other.tensor_)
        , shape_(std::move(other.shape_))
        , policy_(std::move(other.policy_))
    {
        other.tensor_ = nullptr;
    }

    Tensor& operator=(Tensor&& other) noexcept {
        if (this != &other) {
            if (tensor_) TF_DeleteTensor(tensor_);
            tensor_ = other.tensor_;
            shape_ = std::move(other.shape_);
            policy_ = std::move(other.policy_);
            other.tensor_ = nullptr;
        }
        return *this;
    }

    // ─────────────────────────────────────────────────────────────────
    // Queries (no locking - immutable after construction)
    // ─────────────────────────────────────────────────────────────────

    /// Get shape as vector of dimensions
    [[nodiscard]] const std::vector<std::int64_t>& shape() const noexcept {
        return shape_;
    }

    /// Get number of dimensions (rank)
    [[nodiscard]] int rank() const noexcept {
        return static_cast<int>(shape_.size());
    }

    /// Get data type
    [[nodiscard]] TF_DataType dtype() const noexcept {
        return tensor_ ? TF_TensorType(tensor_) : TF_FLOAT;
    }

    /// Get human-readable data type name
    [[nodiscard]] const char* dtype_name() const noexcept {
        return tf::dtype_name(dtype());
    }

    /// Get total byte size
    [[nodiscard]] std::size_t byte_size() const noexcept {
        return tensor_ ? TF_TensorByteSize(tensor_) : 0;
    }

    /// Get number of elements (uses TF API directly - ChatGPT's approach)
    [[nodiscard]] std::size_t num_elements() const {
        if (!tensor_) return 0;
        const std::int64_t n = TF_TensorElementCount(tensor_);
        if (n < 0) {
            throw std::runtime_error("TF_TensorElementCount returned negative value");
        }
        return static_cast<std::size_t>(n);
    }

    /// Check if tensor is empty/null
    [[nodiscard]] bool empty() const noexcept {
        return tensor_ == nullptr;
    }

    /// Explicit bool conversion
    [[nodiscard]] explicit operator bool() const noexcept {
        return tensor_ != nullptr;
    }

    /// Get raw TF_Tensor handle
    [[nodiscard]] TF_Tensor* handle() const noexcept {
        return tensor_;
    }

    // ─────────────────────────────────────────────────────────────────
    // THREAD-SAFE DATA ACCESS (guarded views)
    // ─────────────────────────────────────────────────────────────────

    /// Read access - returns view holding shared lock
    template<TensorScalar T>
    [[nodiscard]] ReadView<T> read() const {
        ensure_tensor_("read");
        auto guard = policy_.scoped_shared();
        ensure_dtype_<T>("read");
        
        const auto n = num_elements();
        const T* ptr = static_cast<const T*>(TF_TensorData(tensor_));
        return ReadView<T>(std::span<const T>(ptr, n), std::move(guard));
    }

    /// Write access - returns view holding exclusive lock
    template<TensorScalar T>
    [[nodiscard]] WriteView<T> write() {
        ensure_tensor_("write");
        auto guard = policy_.scoped_lock();
        ensure_dtype_<T>("write");
        
        const auto n = num_elements();
        T* ptr = static_cast<T*>(TF_TensorData(tensor_));
        return WriteView<T>(std::span<T>(ptr, n), std::move(guard));
    }

    /// Callback-based read access (hardest to misuse)
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
        return static_cast<T*>(TF_TensorData(tensor_));
    }

    template<TensorScalar T>
    [[nodiscard]] const T* unsafe_data() const {
        ensure_tensor_("unsafe_data");
        ensure_dtype_<T>("unsafe_data");
        return static_cast<const T*>(TF_TensorData(tensor_));
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
        return policy_.scoped_shared();
    }
    
    /// Acquire an exclusive (write) lock without accessing data.
    [[nodiscard]] exclusive_guard_type acquire_exclusive_lock() const {
        return policy_.scoped_lock();
    }

    // ─────────────────────────────────────────────────────────────────
    // Factory: FromVector (copies data)
    // ─────────────────────────────────────────────────────────────────

    template<TensorScalar T>
    [[nodiscard]] static Tensor FromVector(
        std::span<const std::int64_t> dims,
        const std::vector<T>& values,
        std::source_location loc = std::source_location::current())
    {
        std::size_t expected = 1;
        for (auto d : dims) {
            if (d < 0) {
                throw std::invalid_argument(std::format(
                    "Tensor::FromVector at {}:{}: negative dimension {}",
                    loc.file_name(), loc.line(), d));
            }
            expected *= static_cast<std::size_t>(d);
        }

        if (expected != values.size()) {
            throw std::invalid_argument(std::format(
                "Tensor::FromVector at {}:{}: shape requires {} elements, got {}",
                loc.file_name(), loc.line(), expected, values.size()));
        }

        const std::size_t bytes = expected * sizeof(T);
        void* mem = std::malloc(bytes);
        if (!mem) throw std::bad_alloc();
        std::memcpy(mem, values.data(), bytes);

        return Tensor(tf_dtype_v<T>, dims, mem, bytes, &default_deallocator, nullptr);
    }

    // ─────────────────────────────────────────────────────────────────
    // Factory: FromScalar
    // ─────────────────────────────────────────────────────────────────

    template<TensorScalar T>
    [[nodiscard]] static Tensor FromScalar(T value) {
        static constexpr std::int64_t dims_arr[1] = {1};
        return FromVector<T>(std::span<const std::int64_t>(dims_arr, 1), 
                            std::vector<T>{value});
    }

    // ─────────────────────────────────────────────────────────────────
    // Factory: FromRaw (adopts ownership)
    // NOTE: No requires-clause - that was a P0 bug!
    // ─────────────────────────────────────────────────────────────────

    [[nodiscard]] static Tensor FromRaw(TF_Tensor* raw) {
        if (!raw) {
            throw std::invalid_argument("Tensor::FromRaw: null TF_Tensor*");
        }

        Tensor t;
        t.tensor_ = raw;  // Adopt ownership

        // Extract shape using CORRECT TF C API
        // NOTE: TF_TensorDims() doesn't exist! Must use TF_NumDims + TF_Dim
        const int ndims = TF_NumDims(raw);
        t.shape_.reserve(static_cast<std::size_t>(ndims));
        for (int i = 0; i < ndims; ++i) {
            t.shape_.push_back(TF_Dim(raw, i));
        }

        return t;
    }

    // ─────────────────────────────────────────────────────────────────
    // Factory: Allocate (uninitialized memory)
    // ─────────────────────────────────────────────────────────────────

    template<TensorScalar T>
    [[nodiscard]] static Tensor Allocate(std::span<const std::int64_t> dims) {
        std::size_t num_elems = 1;
        for (auto d : dims) {
            if (d < 0) throw std::invalid_argument("Negative dimension in Allocate");
            num_elems *= static_cast<std::size_t>(d);
        }

        const std::size_t bytes = num_elems * sizeof(T);
        void* mem = std::malloc(bytes);
        if (!mem) throw std::bad_alloc();

        return Tensor(tf_dtype_v<T>, dims, mem, bytes, &default_deallocator, nullptr);
    }

    // ─────────────────────────────────────────────────────────────────
    // Factory: Zeros (zero-initialized)
    // ─────────────────────────────────────────────────────────────────

    template<TensorScalar T>
    [[nodiscard]] static Tensor Zeros(std::span<const std::int64_t> dims) {
        std::size_t num_elems = 1;
        for (auto d : dims) {
            if (d < 0) throw std::invalid_argument("Negative dimension in Zeros");
            num_elems *= static_cast<std::size_t>(d);
        }

        const std::size_t bytes = num_elems * sizeof(T);
        void* mem = std::calloc(num_elems, sizeof(T));  // Zero-initialized
        if (!mem) throw std::bad_alloc();

        return Tensor(tf_dtype_v<T>, dims, mem, bytes, &default_deallocator, nullptr);
    }

private:
    TF_Tensor* tensor_{nullptr};
    std::vector<std::int64_t> shape_;
    mutable Policy policy_;  // mutable for const read()

    // ─────────────────────────────────────────────────────────────────
    // Private helpers (from ChatGPT - reduces code duplication)
    // ─────────────────────────────────────────────────────────────────

    void ensure_tensor_(const char* fn) const {
        if (!tensor_) {
            throw std::runtime_error(std::format(
                "Tensor::{}(): tensor is null/empty", fn));
        }
    }

    template<TensorScalar T>
    void ensure_dtype_(const char* fn) const {
        const TF_DataType expected = tf_dtype_v<T>;
        const TF_DataType actual = dtype();
        if (actual != expected) {
            throw std::runtime_error(std::format(
                "Tensor::{}(): dtype mismatch - requested {} but tensor is {}",
                fn, tf::dtype_name(expected), tf::dtype_name(actual)));
        }
    }

    static void default_deallocator(void* data, std::size_t, void*) noexcept {
        std::free(data);
    }
};

// ============================================================================
// Type aliases (P3 enhancement)
// ============================================================================

using FastTensor = Tensor<policy::NoLock>;
using SafeTensor = Tensor<policy::Mutex>;
using SharedTensor = Tensor<policy::SharedMutex>;

} // namespace tf
