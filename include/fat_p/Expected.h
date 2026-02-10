#pragma once
/*
FATP_META:
  meta_version: 1
  component: Expected
  file_role: public_header
  path: include/fat_p/Expected.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for Expected."
  api_stability: in_work
  related:
    docs_search: "Expected"
    tests:
      - components/AsyncOperations/tests/test_AsyncOperations.cpp
      - components/Enforce/tests/test_Enforce.cpp
      - components/Expected/tests/test_Expected.cpp
      - components/IdGenerator/tests/test_IdGenerator.cpp
      - components/PipeOperator/tests/test_PipeOperator.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 22
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file Expected.h
 * @brief Production-ready Expected<T,E> with complete monadic operations
 *
 *
 *
 * @section features Key Features
 * - Complete monadic interface (map, and_then, or_else, transform_error)
 * - value_or_else() for lazy defaults
 * - Ordering operators (<, <=, >, >=)
 * - Three-way comparison (C++20)
 * - std::expected integration (C++23)
 * - Feature test macros for capability detection
 * - Rebind template for type transformations
 * - Storage policies (Union/Variant)
 * - Comprehensive noexcept specifications
 * - C++20 minimum, C++23 enhanced
 *
 * @section cpp_versions C++ Version Support
 * - C++20: Full functionality (base implementation with operator<=>)
 * - C++23: + std::expected interoperability (optional)
 *
 * @section differences Differences from std::expected (C++23)
 *
 * **Added Features:**
 * - Storage policies (UnionStorage, VariantStorage)
 * - inspect/inspect_error for non-consuming observation
 * - value_or_else for lazy evaluation
 * - error_or_else for lazy error defaults
 * - Optimized same-state assignment (fast path)
 * - Feature test macros (FATP_EXPECTED_*)
 * - Conversion utilities (to_std_expected, from_std_expected)
 * - Rebind template for type transformations
 * - fold() for pattern matching
 *
 * **Deviations:**
 * - Default error type: std::string (std::expected has no default)
 * - Storage policy customizable (std::expected is implementation-defined)
 *
 * **Compatibility:**
 * - API matches std::expected (drop-in replacement for most code)
 * - Monadic operations identical (map, and_then, or_else, transform_error)
 * - Converting constructors compatible
 *
 * @section migration Migration to std::expected
 * When upgrading to C++23:
 * 1. Use #ifdef __cpp_lib_expected to detect standard availability
 * 2. Use to_std_expected() to interface with std::expected APIs
 * 3. Or: Keep using this implementation for storage policy benefits
 *
 * @section thread_safety Thread Safety
 * Expected objects are NOT thread-safe for concurrent modification.
 *
 * **Safe:** Multiple threads reading same Expected (const operations)
 * **Unsafe:** Concurrent writes or mixing reads/writes (requires synchronization)
 *
 * Use std::atomic<Expected>, std::shared_mutex, or ConcurrencyPolicies.h
 * for thread-safe shared Expected objects.
 *
 * @section complexity Complexity: O(1) for all operations except functors
 * @section exception_safety Exception Safety: Strong guarantee
 */

#include <cassert>     // For assert
#include <concepts>    // For std::constructible_from, std::same_as, etc.
#include <exception>   // Base for bad_expected_access
#include <functional>  // For std::hash
#include <stdexcept>   // For std::logic_error
#include <string>      // Default error type
#include <type_traits> // For std::is_constructible, std::is_same, etc.
#include <utility>     // For std::move, std::forward, etc.

#include "CppFeatureDetection.h"

#ifdef USE_VARIANT_STORAGE
#include <variant> // For VariantStorage policy (conditional use)
#endif

// C++20 three-way comparison support (always available)
#include <compare> // For std::strong_ordering, std::weak_ordering

// C++23 std::expected integration
#if FATP_HAS_EXPECTED
#include <expected> // For std::expected interoperability
#endif

// Conditional include guard for VariantStorage; can be disabled if not needed
// #define USE_VARIANT_STORAGE  // Uncomment to enable VariantStorage for debug scenarios

// =============================================================================
// Feature Test Macros
// =============================================================================

/**
 * @section feature_test_macros Feature Test Macros
 *
 * These macros allow compile-time detection of Expected features.
 * Version format: YYYYMM (e.g., 202411 = November 2024)
 *
 * Usage:
 * @code
 * #if defined(FATP_EXPECTED_MONADIC) && \
 *     FATP_EXPECTED_MONADIC >= 202411L
 *     // Use monadic operations
 *     auto result = exp.map(f).and_then(g);
 * #endif
 * @endcode
 */

// Base Expected implementation (always available)
#ifndef FATP_EXPECTED
#define FATP_EXPECTED 202411L
#endif

// Monadic operations (map, and_then, or_else, transform_error)
#ifndef FATP_EXPECTED_MONADIC
#define FATP_EXPECTED_MONADIC 202411L
#endif

// Storage policy customization (UnionStorage, VariantStorage)
#ifndef FATP_EXPECTED_POLICIES
#define FATP_EXPECTED_POLICIES 202411L
#endif

// Lazy evaluation with value_or_else
#ifndef FATP_EXPECTED_VALUE_OR_ELSE
#define FATP_EXPECTED_VALUE_OR_ELSE 202411L
#endif

// Inspection utilities (inspect, inspect_error)
#ifndef FATP_EXPECTED_INSPECT
#define FATP_EXPECTED_INSPECT 202411L
#endif

// Ordering operators (<, <=, >, >=)
#ifndef FATP_EXPECTED_ORDERING
#define FATP_EXPECTED_ORDERING 202411L
#endif

// Rebind template member
#ifndef FATP_EXPECTED_REBIND
#define FATP_EXPECTED_REBIND 202411L
#endif

// Three-way comparison (C++20, always available)
#ifndef FATP_EXPECTED_SPACESHIP
#define FATP_EXPECTED_SPACESHIP 202411L
#endif

// std::expected integration (C++23, conditionally available)
#if FATP_HAS_EXPECTED
#ifndef FATP_EXPECTED_STD_INTEGRATION
#define FATP_EXPECTED_STD_INTEGRATION 202411L
#endif
#endif

// =============================================================================
// Exception Handling Configuration
// =============================================================================

/**
 * @brief Exception handling for no-exception environments
 *
 * When compiled with -fno-exceptions or equivalent, throwing is replaced with
 * std::terminate(). This allows Expected to be used in HPC, embedded, and
 * other no-exception environments.
 *
 * Detection: Uses __cpp_exceptions (standard), __EXCEPTIONS (GCC/Clang),
 * or _CPPUNWIND (MSVC)
 *
 * Custom Handler: Define FATP_EXPECTED_TERMINATE_HANDLER before including
 * this header to use a custom termination handler in no-exception mode.
 */
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define FATP_EXPECTED_THROW(exception) throw exception
#define FATP_EXPECTED_HAS_EXCEPTIONS 1
#else
#ifndef FATP_EXPECTED_TERMINATE_HANDLER
#define FATP_EXPECTED_TERMINATE_HANDLER() std::terminate()
#endif
#define FATP_EXPECTED_THROW(exception) FATP_EXPECTED_TERMINATE_HANDLER()
#define FATP_EXPECTED_HAS_EXCEPTIONS 0
#endif

namespace fat_p
{

/**
 * @struct unexpect_tag_t
 * @brief A tag type used to signal in-place construction of the error object,
 * mirroring std::unexpect_t in C++23. This disambiguates constructors when
 * types T and E might overlap or be convertible.
 */
struct unexpect_tag_t
{
    explicit unexpect_tag_t() = default;
};
inline constexpr unexpect_tag_t unexpect{}; ///< Inline constant for tag access

/**
 * @class bad_expected_access
 * @brief Exception thrown when attempting to access the value or error of an
 * Expected object that does not contain the requested type.
 * @tparam E The type of the error object contained in Expected.
 *
 * This class preserves the error object for inspection and provides a standard
 * what() message. It is designed to be thrown in value() accessors when the
 * Expected is in an error state.
 */
template <typename E>
class bad_expected_access : public std::exception
{
public:
    /**
     * @brief Constructs the exception with the error object (lvalue reference).
     * @param err Const reference to the error being accessed.
     */
    explicit bad_expected_access(const E& err) noexcept(std::is_nothrow_copy_constructible_v<E>)
        : mError(err)
    {
    }

    /**
     * @brief Constructs the exception with the error object (rvalue reference).
     * @param err Rvalue reference to the error being accessed.
     */
    explicit bad_expected_access(E&& err) noexcept(std::is_nothrow_move_constructible_v<E>)
        : mError(std::move(err))
    {
    }

    /**
     * @brief Accesses the contained error object.
     * @return Const reference to the error.
     */
    const E& error() const& noexcept
    {
        return mError;
    }

    /**
     * @brief Accesses the contained error object (rvalue).
     * @return Rvalue reference to the error.
     */
    E&& error() && noexcept
    {
        return std::move(mError);
    }

    /**
     * @brief Returns a descriptive message for the exception.
     * @return Const char* pointer to the message string.
     */
    const char* what() const noexcept override
    {
        return "bad_expected_access: Attempted to access value when Expected contains error";
    }

private:
    E mError; ///< Stored error object
};

/**
 * @struct unexpected
 * @brief Wrapper for error values to explicitly construct Expected in error
 * state. This helps avoid ambiguity in constructors when T and E are
 * convertible.
 * @tparam E The type of the error object.
 */
template <typename E>
struct unexpected
{
    static_assert(!std::is_same_v<E, void>, "E must not be void");
    static_assert(!std::is_reference_v<E>, "E must not be a reference");
    static_assert(!std::is_array_v<E>, "E must not be an array");

    E mError; ///< The wrapped error value

    /**
     * @brief Constructs from error value.
     * @tparam Err Deduced type of the forwarded error.
     * @param err The error to wrap.
     */
    template <typename Err = E>
        requires (!std::same_as<std::remove_cvref_t<Err>, unexpected> &&
                  !std::same_as<std::remove_cvref_t<Err>, std::in_place_t> &&
                  std::constructible_from<E, Err>)
    constexpr explicit unexpected(Err&& err) noexcept(std::is_nothrow_constructible_v<E, Err>)
        : mError(std::forward<Err>(err))
    {
    }

    /**
     * @brief In-place construction of error.
     */
    template <typename... Args>
        requires std::constructible_from<E, Args...>
    constexpr explicit unexpected(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : mError(std::forward<Args>(args)...)
    {
    }

    // Copy and move
    constexpr unexpected(const unexpected&) = default;
    constexpr unexpected(unexpected&&) = default;
    constexpr unexpected& operator=(const unexpected&) = default;
    constexpr unexpected& operator=(unexpected&&) = default;

    constexpr const E& value() const& noexcept
    {
        return mError;
    }
    constexpr E& value() & noexcept
    {
        return mError;
    }
    constexpr const E&& value() const&& noexcept
    {
        return std::move(mError);
    }
    constexpr E&& value() && noexcept
    {
        return std::move(mError);
    }

    constexpr void swap(unexpected& other) noexcept(std::is_nothrow_swappable_v<E>)
    {
        using std::swap;
        swap(mError, other.mError);
    }
};

// Deduction guide
template <typename E>
unexpected(E) -> unexpected<E>;

/**
 * @brief Factory function to create an unexpected wrapper.
 * @tparam E Type of the error.
 * @param e The error value to wrap.
 * @return unexpected<std::decay_t<E>> The wrapped error.
 */
template <typename E>
constexpr unexpected<std::decay_t<E>> make_unexpected(E&& e)
{
    return unexpected<std::decay_t<E>>(std::forward<E>(e));
}

// Comparison operators for unexpected
template <typename E1, typename E2>
constexpr bool operator==(const unexpected<E1>& lhs, const unexpected<E2>& rhs)
{
    return lhs.value() == rhs.value();
}


// --- Storage Policies ---

/**
 * @struct UnionStorage
 * @brief Policy for manual union-based storage in Expected. Provides
 * zero-overhead storage with manual lifetime management. Ideal for HPC where
 * performance is critical and no additional abstractions are needed.
 * @tparam T Success value type.
 * @tparam E Error type.
 *
 * This policy uses a union for overlapping storage of T and E, with a bool
 * discriminator. It handles construction, destruction, access, and swap
 * explicitly for maximum control and minimal overhead.
 *
 * The mInitialized flag supports non-default-constructible types.
 * Default constructor is always available (doesn't require T to be default-constructible).
 */
template <typename T, typename E>
struct UnionStorage
{
private:
    bool has_value_;   ///< Discriminator: true if value is active
    bool mInitialized; ///< True if union has been initialized (either value or error)
    union
    {
        char mDummy = '\0'; ///< Dummy member for default construction (initialized to avoid UB)
        T mValue;           ///< Storage for success value
        E mError;           ///< Storage for error
    };

    // GCC 14 emits false-positive -Wmaybe-uninitialized for union members when it
    // can't prove which member is active at compile time. This is safe because
    // destroy_active checks has_value_ before accessing any union member.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    void destroy_active() noexcept
    {
        if (mInitialized)
        {
            if (has_value_)
            {
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    mValue.~T();
                }
            }
            else
            {
                if constexpr (!std::is_trivially_destructible_v<E>)
                {
                    mError.~E();
                }
            }
        }
    }
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

public:
    /**
     * @brief Default constructor: Creates uninitialized storage.
     * Always available, even when T is not default-constructible.
     * Union members are NOT constructed until store_value() or store_error() is called.
     * Initializes mDummy member to satisfy C++ Core Guidelines.
     */
    UnionStorage() noexcept
        : has_value_(false)
        , mInitialized(false)
        , mDummy()
    {
        // Union is intentionally left uninitialized (mValue and mError not constructed)
        // Will be initialized by store_value() or store_error()
    }

    /**
     * @brief Destructor: Destroys the active member if initialized.
     */
    ~UnionStorage() noexcept
    {
        destroy_active();
    }

    /**
     * @brief Stores a value, destroying any existing member.
     * Handles uninitialized state correctly.
     * @tparam Args Forwarded arguments for T's constructor.
     */
    template <typename... Args>
    void store_value(Args&&... args)
    {
        destroy_active();
        // Construct new value (placement new required to start object lifetime)
        new (&mValue) T(std::forward<Args>(args)...);
        has_value_ = true;
        mInitialized = true;
    }

    /**
     * @brief Stores an error, destroying any existing member.
     * Handles uninitialized state correctly.
     * @tparam Args Forwarded arguments for E's constructor.
     */
    template <typename... Args>
    void store_error(Args&&... args)
    {
        destroy_active();
        // Construct new error (placement new required to start object lifetime)
        new (&mError) E(std::forward<Args>(args)...);
        has_value_ = false;
        mInitialized = true;
    }

    /**
     * @brief Assigns value directly (without destroying first).
     * Precondition: has_value() must be true AND initialized must be true.
     */
    template <typename Arg>
    void assign_value(Arg&& arg) noexcept(std::is_nothrow_assignable_v<T&, Arg>)
    {
        if constexpr (!std::is_trivial_v<T>)
        {
            assert(has_value_ && mInitialized);
        }
        mValue = std::forward<Arg>(arg);
    }

    /**
     * @brief Assigns error directly (without destroying first).
     * Precondition: has_value() must be false AND initialized must be true.
     */
    template <typename Arg>
    void assign_error(Arg&& arg) noexcept(std::is_nothrow_assignable_v<E&, Arg>)
    {
        if constexpr (!std::is_trivial_v<E>)
        {
            assert(!has_value_ && mInitialized);
        }
        mError = std::forward<Arg>(arg);
    }

    /**
     * @brief Checks if value is active.
     * @return bool True if in value state.
     */
    constexpr bool has_value() const noexcept
    {
        return has_value_ && mInitialized;
    }

    /**
     * @brief Checks if storage has been initialized.
     * @return bool True if either value or error has been constructed.
     */
    constexpr bool is_initialized() const noexcept
    {
        return mInitialized;
    }

    // --- Accessors (throw on uninitialized, assert on wrong state) ---

    constexpr T& get_value() &
    {
        if (!mInitialized)
        {
            FATP_EXPECTED_THROW(std::logic_error("Uninitialized Expected access"));
        }
        assert(has_value_);
        return mValue;
    }
    constexpr const T& get_value() const&
    {
        if (!mInitialized)
        {
            FATP_EXPECTED_THROW(std::logic_error("Uninitialized Expected access"));
        }
        assert(has_value_);
        return mValue;
    }
    constexpr T&& get_value() &&
    {
        if (!mInitialized)
        {
            FATP_EXPECTED_THROW(std::logic_error("Uninitialized Expected access"));
        }
        assert(has_value_);
        return std::move(mValue);
    }
    constexpr const T&& get_value() const&&
    {
        if (!mInitialized)
        {
            FATP_EXPECTED_THROW(std::logic_error("Uninitialized Expected access"));
        }
        assert(has_value_);
        return std::move(mValue);
    }

    constexpr E& get_error() &
    {
        if (!mInitialized)
        {
            FATP_EXPECTED_THROW(std::logic_error("Uninitialized Expected access"));
        }
        assert(!has_value_);
        return mError;
    }
    constexpr const E& get_error() const&
    {
        if (!mInitialized)
        {
            FATP_EXPECTED_THROW(std::logic_error("Uninitialized Expected access"));
        }
        assert(!has_value_);
        return mError;
    }
    constexpr E&& get_error() &&
    {
        if (!mInitialized)
        {
            FATP_EXPECTED_THROW(std::logic_error("Uninitialized Expected access"));
        }
        assert(!has_value_);
        return std::move(mError);
    }
    constexpr const E&& get_error() const&&
    {
        if (!mInitialized)
        {
            FATP_EXPECTED_THROW(std::logic_error("Uninitialized Expected access"));
        }
        assert(!has_value_);
        return std::move(mError);
    }

    /**
     * @brief Swaps with another UnionStorage instance.
     * @param other The other storage to swap with.
     * Handles uninitialized state in both operands.
     */
    // GCC 14 emits false-positive -Wmaybe-uninitialized for union members during swap
    // when the active member hasn't been determined at compile time. This is safe because
    // swap checks has_value_ before accessing any union member.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    void swap(UnionStorage& other) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_swappable_v<T> &&
                                            std::is_nothrow_move_constructible_v<E> && std::is_nothrow_swappable_v<E>)
    {
        // Handle uninitialized cases
        if (!mInitialized && !other.mInitialized)
        {
            return; // Both uninitialized, nothing to swap
        }

        if (!mInitialized)
        {
            // This is uninitialized, other is initialized - move from other to this
            if (other.has_value_)
            {
                store_value(std::move(other.mValue));
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    other.mValue.~T();
                }
            }
            else
            {
                store_error(std::move(other.mError));
                if constexpr (!std::is_trivially_destructible_v<E>)
                {
                    other.mError.~E();
                }
            }
            other.mInitialized = false;
            return;
        }

        if (!other.mInitialized)
        {
            // Other is uninitialized, this is initialized - move from this to other
            if (has_value_)
            {
                other.store_value(std::move(mValue));
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    mValue.~T();
                }
            }
            else
            {
                other.store_error(std::move(mError));
                if constexpr (!std::is_trivially_destructible_v<E>)
                {
                    mError.~E();
                }
            }
            mInitialized = false;
            return;
        }

        // Both initialized - normal swap logic
        if (has_value_ && other.has_value_)
        {
            // Both have values - simple swap
            using std::swap;
            swap(mValue, other.mValue);
        }
        else if (!has_value_ && !other.has_value_)
        {
            // Both have errors - simple swap
            using std::swap;
            swap(mError, other.mError);
        }
        else
        {
            // Different states - need to swap through temp
            if (has_value_)
            {
                T temp_val(std::move(mValue));
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    mValue.~T();
                }
                new (&mError) E(std::move(other.mError));
                if constexpr (!std::is_trivially_destructible_v<E>)
                {
                    other.mError.~E();
                }
                new (&other.mValue) T(std::move(temp_val));
                has_value_ = false;
                other.has_value_ = true;
            }
            else
            {
                E temp_err(std::move(mError));
                if constexpr (!std::is_trivially_destructible_v<E>)
                {
                    mError.~E();
                }
                new (&mValue) T(std::move(other.mValue));
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    other.mValue.~T();
                }
                new (&other.mError) E(std::move(temp_err));
                has_value_ = true;
                other.has_value_ = false;
            }
        }
    }
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
};

/**
 * @struct TrivialStorage
 * @brief Zero-overhead storage policy for trivially copyable types.
 *
 * This policy is designed for HPC scenarios where Expected<int, int> must be
 * passed in CPU registers rather than by pointer. It achieves this by:
 * - No mInitialized flag (assumes always-valid invariant)
 * - Defaulted destructor (enables trivial copyability)
 * - No runtime checks in accessors
 *
 * @tparam T Success value type (must be trivially copyable)
 * @tparam E Error type (must be trivially copyable)
 *
 * @warning Only use with trivially copyable types. Using with non-trivial types
 *          will result in undefined behavior (no destructors called).
 *
 * @note For ABI optimization: Expected<int, int, TrivialStorage> can be passed
 *       in registers on x64 (Itanium ABI), unlike UnionStorage which requires
 *       stack passing due to user-defined destructor.
 */
template <typename T, typename E>
struct TrivialStorage
{
    static_assert(std::is_trivially_copyable_v<T>, "TrivialStorage requires trivially copyable T");
    static_assert(std::is_trivially_copyable_v<E>, "TrivialStorage requires trivially copyable E");

private:
    union
    {
        char mDummy = '\0';
        T mValue;
        E mError;
    };
    bool has_value_ = true; ///< Discriminator (default to value state)

public:
    TrivialStorage() = default;
    ~TrivialStorage() = default;
    TrivialStorage(const TrivialStorage&) = default;
    TrivialStorage(TrivialStorage&&) = default;
    TrivialStorage& operator=(const TrivialStorage&) = default;
    TrivialStorage& operator=(TrivialStorage&&) = default;

    template <typename... Args>
    constexpr explicit TrivialStorage(std::in_place_t, Args&&... args)
        : mValue(std::forward<Args>(args)...)
        , has_value_(true)
    {
    }

    template <typename... Args>
    constexpr explicit TrivialStorage(unexpect_tag_t, Args&&... args)
        : mError(std::forward<Args>(args)...)
        , has_value_(false)
    {
    }

    constexpr bool has_value() const noexcept
    {
        return has_value_;
    }
    constexpr bool is_initialized() const noexcept
    {
        return true;
    }

    constexpr T& get_value() noexcept
    {
        return mValue;
    }
    constexpr const T& get_value() const noexcept
    {
        return mValue;
    }
    constexpr E& get_error() noexcept
    {
        return mError;
    }
    constexpr const E& get_error() const noexcept
    {
        return mError;
    }

    template <typename... Args>
    void store_value(Args&&... args)
    {
        mValue = T(std::forward<Args>(args)...);
        has_value_ = true;
    }

    template <typename... Args>
    void store_error(Args&&... args)
    {
        mError = E(std::forward<Args>(args)...);
        has_value_ = false;
    }

    template <typename Arg>
    void assign_value(Arg&& arg) noexcept
    {
        mValue = std::forward<Arg>(arg);
    }

    template <typename Arg>
    void assign_error(Arg&& arg) noexcept
    {
        mError = std::forward<Arg>(arg);
    }

    void swap(TrivialStorage& other) noexcept
    {
        TrivialStorage temp = *this;
        *this = other;
        other = temp;
    }
};

#ifdef USE_VARIANT_STORAGE

/**
 * @struct VariantStorage
 * @brief Policy using std::variant for storage in Expected. Provides automatic
 * lifetime management and type safety at the cost of potential performance
 * overhead in access and visitation. Useful for simplicity in non-critical
 * paths or debugging.
 * @tparam T Success value type.
 * @tparam E Error type.
 *
 * Wraps std::variant<T, unexpected<E>> to distinguish value and error states.
 * Delegates most operations to variant's methods.
 */
template <typename T, typename E>
struct VariantStorage
{
    using Unexpected = unexpected<E>;
    std::variant<T, Unexpected> data_; ///< Underlying variant storage

    /**
     * @brief Default constructor: Initializes in value state with default T (if T is default-constructible).
     */
    VariantStorage() requires std::default_initializable<T>
        : data_(T{})
    {
    }

    /**
     * @brief Stores a value using emplace.
     * @tparam Args Forwarded arguments for T's constructor.
     */
    template <typename... Args>
    void store_value(Args&&... args)
    {
        data_.template emplace<T>(std::forward<Args>(args)...);
    }

    /**
     * @brief Stores an error using emplace on Unexpected.
     * @tparam Args Forwarded arguments for E's constructor.
     */
    template <typename... Args>
    void store_error(Args&&... args)
    {
        data_.template emplace<Unexpected>(std::forward<Args>(args)...);
    }

    /**
     * @brief Assigns value directly (without emplacing).
     */
    template <typename Arg>
    void assign_value(Arg&& arg)
    {
        assert(data_.index() == 0);
        std::get<T>(data_) = std::forward<Arg>(arg);
    }

    /**
     * @brief Assigns error directly (without emplacing).
     */
    template <typename Arg>
    void assign_error(Arg&& arg)
    {
        assert(data_.index() == 1);
        std::get<Unexpected>(data_).mError = std::forward<Arg>(arg);
    }

    /**
     * @brief Checks if value (index 0) is active.
     * @return bool True if holding T.
     */
    constexpr bool has_value() const noexcept
    {
        return data_.index() == 0;
    }

    /**
     * @brief Checks if storage has been initialized.
     * @return bool Always true for VariantStorage (always initialized).
     */
    constexpr bool is_initialized() const noexcept
    {
        return true;
    }

    // --- Accessors ---

    T& get_value() &
    {
        assert(data_.index() == 0);
        return std::get<T>(data_);
    }
    const T& get_value() const&
    {
        assert(data_.index() == 0);
        return std::get<T>(data_);
    }
    T&& get_value() &&
    {
        assert(data_.index() == 0);
        return std::get<T>(std::move(data_));
    }
    const T&& get_value() const&&
    {
        assert(data_.index() == 0);
        return std::get<T>(std::move(data_));
    }

    E& get_error() &
    {
        assert(data_.index() == 1);
        return std::get<Unexpected>(data_).mError;
    }
    const E& get_error() const&
    {
        assert(data_.index() == 1);
        return std::get<Unexpected>(data_).mError;
    }
    E&& get_error() &&
    {
        assert(data_.index() == 1);
        return std::get<Unexpected>(std::move(data_)).mError;
    }
    const E&& get_error() const&&
    {
        assert(data_.index() == 1);
        return std::get<Unexpected>(std::move(data_)).mError;
    }

    /**
     * @brief Swaps with another VariantStorage instance.
     * @param other The other storage to swap with.
     */
    void swap(VariantStorage& other) noexcept(std::is_nothrow_swappable_v<std::variant<T, Unexpected>>)
    {
        data_.swap(other.data_);
    }
};
#endif

// Forward declaration
template <typename T, typename E, template <typename, typename> class StoragePolicy>
class ExpectedImpl;

// Helper trait for monadic static_asserts
template <typename U, typename Err>
struct is_expected_compatible : std::false_type
{
};

template <typename V, typename Err, template <typename, typename> class SP>
struct is_expected_compatible<ExpectedImpl<V, Err, SP>, Err> : std::true_type
{
};

// Symmetric trait for checking same value type
template <typename U, typename Val>
struct is_expected_with_value : std::false_type
{
};

template <typename Val, typename Err, template <typename, typename> class SP>
struct is_expected_with_value<ExpectedImpl<Val, Err, SP>, Val> : std::true_type
{
};

// Concept wrappers for monadic static_asserts
template <typename U, typename Err>
concept expected_compatible = is_expected_compatible<U, Err>::value;

template <typename U, typename Val>
concept expected_with_value = is_expected_with_value<U, Val>::value;

// --- Helper concepts for constructor constraints ---

namespace detail {

/// T is not constructible or convertible from any cv/ref combination of Other.
/// Guards converting constructors against hijacking by implicit conversions.
template <typename T, typename Other>
concept NotConstructibleFromExpected =
    !std::is_constructible_v<T, Other&> &&
    !std::is_constructible_v<T, const Other&> &&
    !std::is_constructible_v<T, Other&&> &&
    !std::is_constructible_v<T, const Other&&> &&
    !std::is_convertible_v<Other&, T> &&
    !std::is_convertible_v<const Other&, T> &&
    !std::is_convertible_v<Other&&, T> &&
    !std::is_convertible_v<const Other&&, T>;

/// T is not a tag type used for constructor disambiguation.
template <typename T, typename Self>
concept NotTagType =
    !std::same_as<std::remove_cvref_t<T>, std::in_place_t> &&
    !std::same_as<std::remove_cvref_t<T>, Self>;

} // namespace detail



/**
 * @class ExpectedImpl
 * @brief Internal implementation of the Expected monad, templated on storage policy.
 * Users should prefer the Expected alias for the default policy.
 * Supports monadic operations for chaining computations with error handling.
 * @tparam T Success value type (cannot be void; use specialization).
 * @tparam E Error type (defaults to std::string).
 * @tparam StoragePolicy Policy for underlying storage (default: UnionStorage).
 *
 * This class provides a policy-based design for flexibility. It enforces type
 * constraints via static_asserts and uses SFINAE for constructor overloads to
 * handle ambiguities. Monadic functions like map and and_then allow functional
 * error propagation.
 *
 * Constructor ambiguity is resolved by requiring unexpected wrapper for errors.
 * Assignment operators use fast path for same-state assignments.
 * [[nodiscard]] attributes ensure error handling safety.
 * inspect() and inspect_error() provide non-consuming observation.
 */
template <typename T, typename E = std::string, template <typename, typename> class StoragePolicy = UnionStorage>
class [[nodiscard]] ExpectedImpl
{
private:
    StoragePolicy<T, E> mStorage; ///< Policy instance for storage

    static_assert(std::is_destructible_v<T> && std::is_destructible_v<E>, "T and E must be destructible");
    static_assert(!std::is_void_v<T>, "Use ExpectedImpl<void, E> for void success");
    static_assert(!std::is_reference_v<T> && !std::is_array_v<T> && !std::is_function_v<T>,
                  "T must not be reference, array, or function");
    static_assert(!std::is_reference_v<E> && !std::is_array_v<E> && !std::is_function_v<E>,
                  "E must not be reference, array, or function");
    static_assert(!std::is_same_v<T, std::in_place_t> && !std::is_same_v<T, unexpect_tag_t>,
                  "T must not be in_place_t or unexpect_tag_t");
    static_assert(!std::is_same_v<E, std::in_place_t> && !std::is_same_v<E, unexpect_tag_t>,
                  "E must not be in_place_t or unexpect_tag_t");
    // Note: T == E is allowed (e.g., Expected<int, int> for HPC register passing).
    // Constructor ambiguity is avoided via unexpected<E> wrapper for errors.

public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = unexpected<E>;

    /**
     * @brief Rebind value type while keeping error type and storage policy
     * @tparam U New value type
     * @details Type alias for creating Expected with different value type.
     * Useful in generic programming and template metaprogramming.
     *
     * @complexity Compile-time only (zero runtime cost)
     *
     * @code
     * // Example: Transform Expected types in generic code
     * Expected<int, string> int_exp(42);
     * Expected<int, string>::rebind<double> double_exp(3.14);
     *
     * // Both have same error type (string) and storage policy
     * static_assert(std::is_same_v<
     *     decltype(int_exp)::error_type,
     *     decltype(double_exp)::error_type
     * >);
     *
     * // Generic function using rebind
     * template <typename T, typename E>
     * auto convert_to_double(Expected<T, E> exp)
     *     -> typename Expected<T, E>::template rebind<double>
     * {
     *     return exp.map([](const T& x) { return static_cast<double>(x); });
     * }
     * @endcode
     */
    template <typename U>
    using rebind = ExpectedImpl<U, E, StoragePolicy>;

    struct uninitialized_tag
    {
    };

    // --- Constructors ---

    /**
     * @brief Default constructor: Delegates to policy (value state), available only if T is default-constructible.
     */
    constexpr ExpectedImpl() noexcept(std::is_nothrow_default_constructible_v<T>)
        requires std::default_initializable<T>
    {
        mStorage.store_value();
    }

    constexpr ExpectedImpl(uninitialized_tag) noexcept
        : mStorage()
    {
    }

    /**
     * @brief Copy constructs from T.
     * @param v Const reference to T.
     */
    template <typename U = T>
        requires (std::constructible_from<T, const U&> &&
                  detail::NotTagType<U, ExpectedImpl>)
    constexpr ExpectedImpl(const U& v) noexcept(std::is_nothrow_constructible_v<T, const U&>)
    {
        mStorage.store_value(v);
    }

    /**
     * @brief Move constructs from T.
     * @param v Rvalue reference to T.
     */
    template <typename U = T>
        requires (std::constructible_from<T, U&&> &&
                  detail::NotTagType<U, ExpectedImpl>)
    constexpr ExpectedImpl(U&& v) noexcept(std::is_nothrow_constructible_v<T, U&&>)
    {
        mStorage.store_value(std::forward<U>(v));
    }

    /**
     * @brief In-place value construction.
     * @tparam Args Arguments for T's constructor.
     */
    template <typename... Args>
        requires std::constructible_from<T, Args...>
    constexpr explicit ExpectedImpl(std::in_place_t,
                                    Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        mStorage.store_value(std::forward<Args>(args)...);
    }

    /**
     * @brief In-place value construction with initializer list.
     */
    template <typename U,
              typename... Args>
        requires std::constructible_from<T, std::initializer_list<U>&, Args...>
    constexpr explicit ExpectedImpl(std::in_place_t, std::initializer_list<U> il, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<T, std::initializer_list<U>&, Args...>)
    {
        mStorage.store_value(il, std::forward<Args>(args)...);
    }

    /**
     * @brief In-place error construction using custom tag.
     * @tparam Args Arguments for E's constructor.
     *
     * This is the primary way to construct errors, eliminating ambiguity.
     */
    template <typename... Args>
        requires std::constructible_from<E, Args...>
    constexpr explicit ExpectedImpl(unexpect_tag_t,
                                    Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
    {
        mStorage.store_error(std::forward<Args>(args)...);
    }

    /**
     * @brief In-place error construction with initializer list.
     */
    template <typename U,
              typename... Args>
        requires std::constructible_from<E, std::initializer_list<U>&, Args...>
    constexpr explicit ExpectedImpl(unexpect_tag_t, std::initializer_list<U> il, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<E, std::initializer_list<U>&, Args...>)
    {
        mStorage.store_error(il, std::forward<Args>(args)...);
    }

    /**
     * @brief Constructs from const unexpected (copy).
     * @tparam G Type convertible to E.
     * @param ue The unexpected wrapper.
     *
     * Unambiguous way to construct error state.
     */
    template <typename G>
        requires std::constructible_from<E, const G&>
    constexpr ExpectedImpl(const unexpected<G>& ue) noexcept(std::is_nothrow_constructible_v<E, const G&>)
    {
        mStorage.store_error(ue.value());
    }

    /**
     * @brief Constructs from rvalue unexpected (move).
     * @tparam G Type convertible to E.
     * @param ue The unexpected wrapper.
     */
    template <typename G>
        requires std::constructible_from<E, G&&>
    constexpr ExpectedImpl(unexpected<G>&& ue) noexcept(std::is_nothrow_constructible_v<E, G&&>)
    {
        mStorage.store_error(std::move(ue).value());
    }

    // --- Copy/Move Constructors and Assignments ---

    /**
     * @brief Copy constructor: Copies from another's storage.
     * @param other The other ExpectedImpl.
     */
    ExpectedImpl(const ExpectedImpl& other) noexcept(std::is_nothrow_copy_constructible_v<T> &&
                                                     std::is_nothrow_copy_constructible_v<E>)
    {
        if (other.has_value())
        {
            mStorage.store_value(other.mStorage.get_value());
        }
        else
        {
            mStorage.store_error(other.mStorage.get_error());
        }
    }

    /**
     * @brief Move constructor: Moves from another's storage.
     * @param other The other ExpectedImpl.
     */
    ExpectedImpl(ExpectedImpl&& other) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                                std::is_nothrow_move_constructible_v<E>)
    {
        if (other.has_value())
        {
            mStorage.store_value(std::move(other.mStorage.get_value()));
        }
        else
        {
            mStorage.store_error(std::move(other.mStorage.get_error()));
        }
    }

    /**
     * @brief Converting copy constructor.
     */
    template <
        typename U,
        typename G,
        template <typename, typename> class SP>
        requires (std::constructible_from<T, const U&> && std::constructible_from<E, const G&> &&
                  detail::NotConstructibleFromExpected<T, ExpectedImpl<U, G, SP>>)
    explicit ExpectedImpl(const ExpectedImpl<U, G, SP>& other) noexcept(std::is_nothrow_constructible_v<T, const U&> &&
                                                                        std::is_nothrow_constructible_v<E, const G&>)
    {
        if (other.has_value())
        {
            mStorage.store_value(other.mStorage.get_value());
        }
        else
        {
            mStorage.store_error(other.mStorage.get_error());
        }
    }

    /**
     * @brief Converting move constructor.
     */
    template <typename U,
              typename G,
              template <typename, typename> class SP>
        requires (std::constructible_from<T, U&&> && std::constructible_from<E, G&&> &&
                  detail::NotConstructibleFromExpected<T, ExpectedImpl<U, G, SP>>)
    explicit ExpectedImpl(ExpectedImpl<U, G, SP>&& other) noexcept(std::is_nothrow_constructible_v<T, U&&> &&
                                                                   std::is_nothrow_constructible_v<E, G&&>)
    {
        if (other.has_value())
        {
            mStorage.store_value(std::move(other.mStorage.get_value()));
        }
        else
        {
            mStorage.store_error(std::move(other.mStorage.get_error()));
        }
    }

    /**
     * @brief Copy assignment.
     * @param other The other ExpectedImpl.
     * @return Reference to this.
     *
     * Uses fast path for same-state assignments.
     */
    ExpectedImpl& operator=(const ExpectedImpl& other) noexcept(std::is_nothrow_copy_constructible_v<T> &&
                                                                std::is_nothrow_copy_constructible_v<E> &&
                                                                std::is_nothrow_copy_assignable_v<T> &&
                                                                std::is_nothrow_copy_assignable_v<E>)
    {
        if (this != &other)
        {
            const bool this_has_val = has_value();
            const bool other_has_val = other.has_value();

            if (this_has_val == other_has_val)
            {
                // Fast path: Same state - direct assignment (no destructor calls)
                if (this_has_val)
                {
                    mStorage.assign_value(other.mStorage.get_value());
                }
                else
                {
                    mStorage.assign_error(other.mStorage.get_error());
                }
            }
            else
            {
                // Slow path: Different states - need destroy + construct
                if (other_has_val)
                {
                    mStorage.store_value(other.mStorage.get_value());
                }
                else
                {
                    mStorage.store_error(other.mStorage.get_error());
                }
            }
        }
        return *this;
    }

    /**
     * @brief Move assignment.
     * @param other The other ExpectedImpl.
     * @return Reference to this.
     *
     * Uses fast path for same-state assignments (2-10x speedup).
     */
    ExpectedImpl& operator=(ExpectedImpl&& other) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                                           std::is_nothrow_move_constructible_v<E> &&
                                                           std::is_nothrow_move_assignable_v<T> &&
                                                           std::is_nothrow_move_assignable_v<E>)
    {
        if (this != &other)
        {
            const bool this_has_val = has_value();
            const bool other_has_val = other.has_value();

            if (this_has_val == other_has_val)
            {
                // Fast path: Same state - direct assignment (no destructor calls)
                if (this_has_val)
                {
                    mStorage.assign_value(std::move(other.mStorage.get_value()));
                }
                else
                {
                    mStorage.assign_error(std::move(other.mStorage.get_error()));
                }
            }
            else
            {
                // Slow path: Different states - need destroy + construct
                if (other_has_val)
                {
                    mStorage.store_value(std::move(other.mStorage.get_value()));
                }
                else
                {
                    mStorage.store_error(std::move(other.mStorage.get_error()));
                }
            }
        }
        return *this;
    }

    /**
     * @brief Assignment from T (copy).
     * @param v Const reference to T.
     * @return Reference to this.
     */
    template <typename U = T>
        requires (!std::same_as<std::remove_cvref_t<U>, ExpectedImpl> &&
                  std::constructible_from<T, U> && std::is_assignable_v<T&, U>)
    ExpectedImpl& operator=(U&& v) noexcept(std::is_nothrow_constructible_v<T, U> &&
                                            std::is_nothrow_assignable_v<T&, U>)
    {
        if (has_value())
        {
            mStorage.assign_value(std::forward<U>(v));
        }
        else
        {
            mStorage.store_value(std::forward<U>(v));
        }
        return *this;
    }

    /**
     * @brief Assignment from unexpected.
     * @tparam G Type of error in unexpected.
     * @param ue The unexpected wrapper.
     * @return Reference to this.
     */
    template <typename G>
    ExpectedImpl& operator=(const unexpected<G>& ue) noexcept(std::is_nothrow_constructible_v<E, const G&> &&
                                                              std::is_nothrow_assignable_v<E&, const G&>)
    {
        if (has_value())
        {
            mStorage.store_error(ue.value());
        }
        else
        {
            mStorage.assign_error(ue.value());
        }
        return *this;
    }

    template <typename G>
    ExpectedImpl& operator=(unexpected<G>&& ue) noexcept(std::is_nothrow_constructible_v<E, G&&> &&
                                                         std::is_nothrow_assignable_v<E&, G&&>)
    {
        if (has_value())
        {
            mStorage.store_error(std::move(ue).value());
        }
        else
        {
            mStorage.assign_error(std::move(ue).value());
        }
        return *this;
    }

    // --- Emplace ---

    /**
     * @brief Emplaces a value in-place.
     * @tparam Args Arguments for T's constructor.
     * @return Reference to the emplaced value.
     */
    template <typename... Args>
    T& emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        mStorage.store_value(std::forward<Args>(args)...);
        return mStorage.get_value();
    }

    template <typename U, typename... Args>
    T& emplace(std::initializer_list<U> il,
               Args&&... args) noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<U>&, Args...>)
    {
        mStorage.store_value(il, std::forward<Args>(args)...);
        return mStorage.get_value();
    }

    // --- Swap ---

    /**
     * @brief Swaps with another ExpectedImpl.
     * @param other The other ExpectedImpl.
     */
    void swap(ExpectedImpl& other) noexcept(noexcept(mStorage.swap(other.mStorage)))
    {
        mStorage.swap(other.mStorage);
    }

    // --- Observers ---

    /**
     * @brief Checks if in value state.
     * @return bool True if has value.
     */
    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return mStorage.has_value();
    }

    /**
     * @brief Checks if in error state.
     * @return bool True if has error.
     */
    [[nodiscard]] constexpr bool has_error() const noexcept
    {
        return !mStorage.has_value();
    }

    /**
     * @brief Implicit bool conversion for success check.
     */
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    // --- Unchecked Accessors ---

    constexpr T* operator->() noexcept
    {
        assert(has_value());
        return &mStorage.get_value();
    }
    constexpr const T* operator->() const noexcept
    {
        assert(has_value());
        return &mStorage.get_value();
    }

    constexpr T& operator*() & noexcept
    {
        assert(has_value());
        return mStorage.get_value();
    }
    constexpr const T& operator*() const& noexcept
    {
        assert(has_value());
        return mStorage.get_value();
    }
    constexpr T&& operator*() && noexcept
    {
        assert(has_value());
        return std::move(mStorage.get_value());
    }
    constexpr const T&& operator*() const&& noexcept
    {
        assert(has_value());
        return std::move(mStorage.get_value());
    }

    // --- Unchecked Value Accessors (for verified hot paths) ---

    /**
     * @brief Direct value access without any checks.
     *
     * Use only when has_value() has been verified externally.
     * Provides maximum performance for inner loops where state is known.
     *
     * @warning Undefined behavior if !has_value()
     */
    [[nodiscard]] constexpr T& value_unchecked() & noexcept
    {
        return mStorage.get_value();
    }
    [[nodiscard]] constexpr const T& value_unchecked() const& noexcept
    {
        return mStorage.get_value();
    }
    [[nodiscard]] constexpr T&& value_unchecked() && noexcept
    {
        return std::move(mStorage.get_value());
    }
    [[nodiscard]] constexpr const T&& value_unchecked() const&& noexcept
    {
        return std::move(mStorage.get_value());
    }

    // --- Throwing Value Accessors ---

    [[nodiscard]] constexpr T& value() &
    {
        if (!has_value())
        {
            FATP_EXPECTED_THROW(bad_expected_access<E>(mStorage.get_error()));
        }
        return mStorage.get_value();
    }
    [[nodiscard]] constexpr const T& value() const&
    {
        if (!has_value())
        {
            FATP_EXPECTED_THROW(bad_expected_access<E>(mStorage.get_error()));
        }
        return mStorage.get_value();
    }
    [[nodiscard]] constexpr T&& value() &&
    {
        if (!has_value())
        {
            FATP_EXPECTED_THROW(bad_expected_access<E>(std::move(mStorage.get_error())));
        }
        return std::move(mStorage.get_value());
    }
    [[nodiscard]] constexpr const T&& value() const&&
    {
        if (!has_value())
        {
            FATP_EXPECTED_THROW(bad_expected_access<E>(mStorage.get_error()));
        }
        return std::move(mStorage.get_value());
    }

    // --- Error Accessors (unchecked) ---

    constexpr E& error() & noexcept
    {
        assert(!has_value());
        return mStorage.get_error();
    }
    constexpr const E& error() const& noexcept
    {
        assert(!has_value());
        return mStorage.get_error();
    }
    constexpr E&& error() && noexcept
    {
        assert(!has_value());
        return std::move(mStorage.get_error());
    }
    constexpr const E&& error() const&& noexcept
    {
        assert(!has_value());
        return std::move(mStorage.get_error());
    }

    // --- Fallbacks ---

    /**
     * @brief Returns value or default if error.
     * @tparam U Type of default.
     * @param default_value Forwarded default.
     * @return T The value or default.
     */
    template <typename U>
    [[nodiscard]] constexpr T value_or(U&& default_value) const&
    {
        return has_value() ? mStorage.get_value() : static_cast<T>(std::forward<U>(default_value));
    }

    template <typename U>
    [[nodiscard]] constexpr T value_or(U&& default_value) &&
    {
        return has_value() ? std::move(mStorage.get_value()) : static_cast<T>(std::forward<U>(default_value));
    }


    /**
     * @brief Returns value or evaluates functor (lazy evaluation)
     * @complexity O(1) if value, O(f) if error
     */
    template <typename F>
    [[nodiscard]] constexpr T value_or_else(F&& f) const&
    {
        return has_value() ? **this : std::forward<F>(f)();
    }

    template <typename F>
    [[nodiscard]] constexpr T value_or_else(F&& f) &&
    {
        return has_value() ? std::move(**this) : std::forward<F>(f)();
    }

    /**
     * @brief Returns error or default if value.
     * @tparam G Type of default error.
     * @param default_error Forwarded default.
     * @return E The error or default.
     */
    template <typename G>
    [[nodiscard]] constexpr E error_or(G&& default_error) const&
    {
        return has_value() ? static_cast<E>(std::forward<G>(default_error)) : mStorage.get_error();
    }

    template <typename G>
    [[nodiscard]] constexpr E error_or(G&& default_error) &&
    {
        return has_value() ? static_cast<E>(std::forward<G>(default_error)) : std::move(mStorage.get_error());
    }

    /**
     * @brief Returns error or evaluates functor (lazy evaluation)
     * @tparam F Callable type returning E
     * @param f The factory function for default error
     * @return E The error or result of f()
     * @complexity O(1) if error, O(f) if value
     */
    template <typename F>
    [[nodiscard]] constexpr E error_or_else(F&& f) const&
    {
        return has_value() ? std::forward<F>(f)() : mStorage.get_error();
    }

    template <typename F>
    [[nodiscard]] constexpr E error_or_else(F&& f) &&
    {
        return has_value() ? std::forward<F>(f)() : std::move(mStorage.get_error());
    }

    // --- Monadic Functions (Success Path) ---

    /**
     * @brief Maps value with function if present.
     * @tparam F Function type.
     * @param f The mapping function.
     * @return ExpectedImpl<U, E> Transformed or error.
     */
    template <typename F>
    [[nodiscard]] constexpr auto map(F&& f) &
    {
        using U = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
        if (has_value())
        {
            return ExpectedImpl<U, E, StoragePolicy>(std::in_place,
                                                     std::invoke(std::forward<F>(f), mStorage.get_value()));
        }
        return ExpectedImpl<U, E, StoragePolicy>(unexpect, mStorage.get_error());
    }

    template <typename F>
    [[nodiscard]] constexpr auto map(F&& f) const&
    {
        using U = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
        if (has_value())
        {
            return ExpectedImpl<U, E, StoragePolicy>(std::in_place,
                                                     std::invoke(std::forward<F>(f), mStorage.get_value()));
        }
        return ExpectedImpl<U, E, StoragePolicy>(unexpect, mStorage.get_error());
    }

    template <typename F>
    [[nodiscard]] constexpr auto map(F&& f) &&
    {
        using U = std::invoke_result_t<F, T&&>;
        static_assert(std::is_constructible_v<U, decltype(std::invoke(std::forward<F>(f), std::declval<T&&>()))>);
        if (has_value())
        {
            return ExpectedImpl<std::decay_t<U>, E, StoragePolicy>(
                std::in_place,
                std::invoke(std::forward<F>(f), std::move(mStorage.get_value())));
        }
        return ExpectedImpl<std::decay_t<U>, E, StoragePolicy>(unexpect, std::move(mStorage.get_error()));
    }

    template <typename F>
    [[nodiscard]] constexpr auto map(F&& f) const&&
    {
        using U = std::remove_cvref_t<std::invoke_result_t<F, const T&&>>;
        if (has_value())
        {
            return ExpectedImpl<U, E, StoragePolicy>(std::in_place,
                                                     std::invoke(std::forward<F>(f), std::move(mStorage.get_value())));
        }
        return ExpectedImpl<U, E, StoragePolicy>(unexpect, std::move(mStorage.get_error()));
    }

    template <typename F>
    [[nodiscard]] constexpr auto transform(F&& f) &
    {
        return map(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto transform(F&& f) const&
    {
        return map(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto transform(F&& f) &&
    {
        return std::move(*this).map(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto transform(F&& f) const&&
    {
        return std::move(*this).map(std::forward<F>(f));
    }

    /**
     * @brief Monadic bind operation (flatMap).
     * @tparam F Function type returning Expected<U, E>.
     * @param f The function to apply.
     * @return Expected<U, E> Result of f or propagated error.
     */
    template <typename F>
    [[nodiscard]] constexpr auto and_then(F&& f) &
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
        static_assert(expected_compatible<Result, E>, "and_then must return ExpectedImpl<U, E> with same E");
        if (has_value())
        {
            return std::invoke(std::forward<F>(f), mStorage.get_value());
        }
        return Result(unexpect, mStorage.get_error());
    }

    template <typename F>
    [[nodiscard]] constexpr auto and_then(F&& f) const&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
        static_assert(expected_compatible<Result, E>, "and_then must return ExpectedImpl<U, E> with same E");
        if (has_value())
        {
            return std::invoke(std::forward<F>(f), mStorage.get_value());
        }
        return Result(unexpect, mStorage.get_error());
    }

    template <typename F>
    [[nodiscard]] constexpr auto and_then(F&& f) &&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, T&&>>;
        static_assert(expected_compatible<Result, E>, "and_then must return ExpectedImpl<U, E> with same E");
        if (has_value())
        {
            return std::invoke(std::forward<F>(f), std::move(mStorage.get_value()));
        }
        return Result(unexpect, std::move(mStorage.get_error()));
    }

    template <typename F>
    [[nodiscard]] constexpr auto and_then(F&& f) const&&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, const T&&>>;
        static_assert(expected_compatible<Result, E>, "and_then must return ExpectedImpl<U, E> with same E");
        if (has_value())
        {
            return std::invoke(std::forward<F>(f), std::move(mStorage.get_value()));
        }
        return Result(unexpect, std::move(mStorage.get_error()));
    }

    template <typename F>
    [[nodiscard]] constexpr auto flat_map(F&& f) &
    {
        return and_then(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto flat_map(F&& f) const&
    {
        return and_then(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto flat_map(F&& f) &&
    {
        return std::move(*this).and_then(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto flat_map(F&& f) const&&
    {
        return std::move(*this).and_then(std::forward<F>(f));
    }

    // --- Monadic Functions (Error Path) ---

    /**
     * @brief Maps error with function if present.
     * @tparam F Function type.
     * @param f The mapping function.
     * @return ExpectedImpl<T, G> Value or transformed error.
     */
    template <typename F>
    [[nodiscard]] constexpr auto map_error(F&& f) &
    {
        using G = std::remove_cvref_t<std::invoke_result_t<F, E&>>;
        if (!has_value())
        {
            return ExpectedImpl<T, G, StoragePolicy>(unexpect, std::invoke(std::forward<F>(f), mStorage.get_error()));
        }
        return ExpectedImpl<T, G, StoragePolicy>(std::in_place, mStorage.get_value());
    }

    template <typename F>
    [[nodiscard]] constexpr auto map_error(F&& f) const&
    {
        using G = std::remove_cvref_t<std::invoke_result_t<F, const E&>>;
        if (!has_value())
        {
            return ExpectedImpl<T, G, StoragePolicy>(unexpect, std::invoke(std::forward<F>(f), mStorage.get_error()));
        }
        return ExpectedImpl<T, G, StoragePolicy>(std::in_place, mStorage.get_value());
    }

    template <typename F>
    [[nodiscard]] constexpr auto map_error(F&& f) &&
    {
        using G = std::remove_cvref_t<std::invoke_result_t<F, E&&>>;
        if (!has_value())
        {
            return ExpectedImpl<T, G, StoragePolicy>(unexpect,
                                                     std::invoke(std::forward<F>(f), std::move(mStorage.get_error())));
        }
        return ExpectedImpl<T, G, StoragePolicy>(std::in_place, std::move(mStorage.get_value()));
    }

    template <typename F>
    [[nodiscard]] constexpr auto map_error(F&& f) const&&
    {
        using G = std::remove_cvref_t<std::invoke_result_t<F, const E&&>>;
        if (!has_value())
        {
            return ExpectedImpl<T, G, StoragePolicy>(unexpect,
                                                     std::invoke(std::forward<F>(f), std::move(mStorage.get_error())));
        }
        return ExpectedImpl<T, G, StoragePolicy>(std::in_place, std::move(mStorage.get_value()));
    }

    template <typename F>
    [[nodiscard]] constexpr auto transform_error(F&& f) &
    {
        return map_error(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto transform_error(F&& f) const&
    {
        return map_error(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto transform_error(F&& f) &&
    {
        return std::move(*this).map_error(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto transform_error(F&& f) const&&
    {
        return std::move(*this).map_error(std::forward<F>(f));
    }

    /**
     * @brief Error recovery operation.
     * @tparam F Function type returning Expected<T, G>.
     * @param f The recovery function.
     * @return Expected<T, G> Value or result of f.
     */
    template <typename F>
    [[nodiscard]] constexpr auto or_else(F&& f) &
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, E&>>;
        static_assert(expected_with_value<Result, T>, "or_else must return ExpectedImpl<T, G> with same T");
        if (!has_value())
        {
            return std::invoke(std::forward<F>(f), mStorage.get_error());
        }
        return Result(std::in_place, mStorage.get_value());
    }

    template <typename F>
    [[nodiscard]] constexpr auto or_else(F&& f) const&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, const E&>>;
        static_assert(expected_with_value<Result, T>, "or_else must return ExpectedImpl<T, G> with same T");
        if (!has_value())
        {
            return std::invoke(std::forward<F>(f), mStorage.get_error());
        }
        return Result(std::in_place, mStorage.get_value());
    }

    template <typename F>
    [[nodiscard]] constexpr auto or_else(F&& f) &&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, E&&>>;
        static_assert(expected_with_value<Result, T>, "or_else must return ExpectedImpl<T, G> with same T");
        if (!has_value())
        {
            return std::invoke(std::forward<F>(f), std::move(mStorage.get_error()));
        }
        return Result(std::in_place, std::move(mStorage.get_value()));
    }

    template <typename F>
    [[nodiscard]] constexpr auto or_else(F&& f) const&&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, const E&&>>;
        static_assert(expected_with_value<Result, T>, "or_else must return ExpectedImpl<T, G> with same T");
        if (!has_value())
        {
            return std::invoke(std::forward<F>(f), std::move(mStorage.get_error()));
        }
        return Result(std::in_place, std::move(mStorage.get_value()));
    }

    // --- Inspection (Non-consuming observation) ---

    /**
     * @brief Inspect value without consuming (for side effects like logging).
     * @tparam F Function type taking const T&.
     * @param f The inspection function.
     * @return Reference to this (for chaining).
     */
    template <typename F>
    constexpr const ExpectedImpl& inspect(F&& f) const&
    {
        if (has_value())
        {
            std::invoke(std::forward<F>(f), mStorage.get_value());
        }
        return *this;
    }

    template <typename F>
    constexpr ExpectedImpl& inspect(F&& f) &
    {
        if (has_value())
        {
            std::invoke(std::forward<F>(f), mStorage.get_value());
        }
        return *this;
    }

    /**
     * @brief Inspect error without consuming.
     * @tparam F Function type taking const E&.
     * @param f The inspection function.
     * @return Reference to this (for chaining).
     */
    template <typename F>
    constexpr const ExpectedImpl& inspect_error(F&& f) const&
    {
        if (!has_value())
        {
            std::invoke(std::forward<F>(f), mStorage.get_error());
        }
        return *this;
    }

    template <typename F>
    constexpr ExpectedImpl& inspect_error(F&& f) &
    {
        if (!has_value())
        {
            std::invoke(std::forward<F>(f), mStorage.get_error());
        }
        return *this;
    }

    // --- Fold/Match for pattern matching ---

    template <typename ValF, typename ErrF>
    [[nodiscard]] constexpr auto fold(ValF&& val_f, ErrF&& err_f) const&
    {
        if (has_value())
        {
            return std::invoke(std::forward<ValF>(val_f), mStorage.get_value());
        }
        return std::invoke(std::forward<ErrF>(err_f), mStorage.get_error());
    }

    template <typename ValF, typename ErrF>
    [[nodiscard]] constexpr auto fold(ValF&& val_f, ErrF&& err_f) &
    {
        if (has_value())
        {
            return std::invoke(std::forward<ValF>(val_f), mStorage.get_value());
        }
        return std::invoke(std::forward<ErrF>(err_f), mStorage.get_error());
    }

    template <typename ValF, typename ErrF>
    [[nodiscard]] constexpr auto fold(ValF&& val_f, ErrF&& err_f) &&
    {
        if (has_value())
        {
            return std::invoke(std::forward<ValF>(val_f), std::move(mStorage.get_value()));
        }
        return std::invoke(std::forward<ErrF>(err_f), std::move(mStorage.get_error()));
    }

    template <typename ValF, typename ErrF>
    [[nodiscard]] constexpr auto fold(ValF&& val_f, ErrF&& err_f) const&&
    {
        if (has_value())
        {
            return std::invoke(std::forward<ValF>(val_f), std::move(mStorage.get_value()));
        }
        return std::invoke(std::forward<ErrF>(err_f), std::move(mStorage.get_error()));
    }
};

// --- Comparison Operators ---

/**
 * @brief Equality comparison for ExpectedImpl.
 */
template <typename T1,
          typename E1,
          typename T2,
          typename E2,
          template <typename, typename> class SP1,
          template <typename, typename> class SP2>
[[nodiscard]] constexpr bool operator==(const ExpectedImpl<T1, E1, SP1>& lhs, const ExpectedImpl<T2, E2, SP2>& rhs)
{
    if (lhs.has_value() != rhs.has_value())
    {
        return false;
    }
    if (lhs.has_value())
    {
        return *lhs == *rhs;
    }
    return lhs.error() == rhs.error();
}


// Comparison with T
template <typename T1, typename E1, typename T2, template <typename, typename> class SP>
[[nodiscard]] constexpr bool operator==(const ExpectedImpl<T1, E1, SP>& lhs, const T2& rhs)
{
    return lhs.has_value() && *lhs == rhs;
}

template <typename T1, typename E1, typename T2, template <typename, typename> class SP>
[[nodiscard]] constexpr bool operator==(const T1& lhs, const ExpectedImpl<T2, E1, SP>& rhs)
{
    return rhs.has_value() && lhs == *rhs;
}



// Comparison with unexpected
template <typename T, typename E1, typename E2, template <typename, typename> class SP>
[[nodiscard]] constexpr bool operator==(const ExpectedImpl<T, E1, SP>& lhs, const unexpected<E2>& rhs)
{
    return !lhs.has_value() && lhs.error() == rhs.value();
}

template <typename T, typename E1, typename E2, template <typename, typename> class SP>
[[nodiscard]] constexpr bool operator==(const unexpected<E1>& lhs, const ExpectedImpl<T, E2, SP>& rhs)
{
    return !rhs.has_value() && lhs.value() == rhs.error();
}



// --- Void Specialization Storage Policies ---

/**
 * @struct UnionStorage<void, E>
 * @brief Specialized UnionStorage for void success type.
 * @tparam E Error type.
 */
template <typename E>
struct UnionStorage<void, E>
{
private:
    bool has_value_;   ///< Discriminator (true for success)
    bool mInitialized; ///< True if union has been initialized (either value or error)
    union
    {
        char mDummy = '\0'; ///< Dummy member for default construction (initialized to avoid UB)
        E mError;           ///< Error storage (active when !has_value_)
    };

    void destroy_active() noexcept
    {
        if (mInitialized && !has_value_)
        {
            if constexpr (!std::is_trivially_destructible_v<E>)
            {
                mError.~E();
            }
        }
    }

public:
    UnionStorage() noexcept
        : has_value_(true)
        , mInitialized(true)
        , mDummy()
    {
    }

    ~UnionStorage() noexcept
    {
        destroy_active();
    }

    void store_value()
    {
        destroy_active();
        has_value_ = true;
        mInitialized = true;
    }

    template <typename... Args>
    void store_error(Args&&... args)
    {
        destroy_active();
        if constexpr (sizeof...(Args) == 0 && std::is_trivially_default_constructible_v<E>)
        {
            // No placement new for trivial
        }
        else
        {
            new (&mError) E(std::forward<Args>(args)...);
        }
        has_value_ = false;
        mInitialized = true;
    }

    template <typename Arg>
    void assign_error(Arg&& arg) noexcept(std::is_nothrow_assignable_v<E&, Arg>)
    {
        assert(!has_value_);
        mError = std::forward<Arg>(arg);
    }

    constexpr bool has_value() const noexcept
    {
        return has_value_ && mInitialized;
    }

    constexpr bool is_initialized() const noexcept
    {
        return mInitialized;
    }

    // No get_value for void

    constexpr E& get_error() &
    {
        if (!mInitialized)
        {
            FATP_EXPECTED_THROW(std::logic_error("Uninitialized Expected access"));
        }
        assert(!has_value_);
        return mError;
    }
    constexpr const E& get_error() const&
    {
        if (!mInitialized)
        {
            FATP_EXPECTED_THROW(std::logic_error("Uninitialized Expected access"));
        }
        assert(!has_value_);
        return mError;
    }
    constexpr E&& get_error() &&
    {
        if (!mInitialized)
        {
            FATP_EXPECTED_THROW(std::logic_error("Uninitialized Expected access"));
        }
        assert(!has_value_);
        return std::move(mError);
    }
    constexpr const E&& get_error() const&&
    {
        if (!mInitialized)
        {
            FATP_EXPECTED_THROW(std::logic_error("Uninitialized Expected access"));
        }
        assert(!has_value_);
        return std::move(mError);
    }

    void swap(UnionStorage& other) noexcept(std::is_nothrow_move_constructible_v<E> && std::is_nothrow_swappable_v<E>)
    {
        if (!mInitialized && !other.mInitialized)
        {
            return;
        }

        if (!mInitialized)
        {
            // This uninit, other init
            if (other.has_value_)
            {
                store_value();
                other.mInitialized = false;
            }
            else
            {
                store_error(std::move(other.mError));
                if constexpr (!std::is_trivially_destructible_v<E>)
                {
                    other.mError.~E();
                }
                other.mInitialized = false;
            }
            return;
        }

        if (!other.mInitialized)
        {
            // Other uninit, this init
            if (has_value_)
            {
                other.store_value();
                mInitialized = false;
            }
            else
            {
                other.store_error(std::move(mError));
                if constexpr (!std::is_trivially_destructible_v<E>)
                {
                    mError.~E();
                }
                mInitialized = false;
            }
            return;
        }

        // Both init
        if (has_value_ == other.has_value_)
        {
            if (!has_value_)
            {
                using std::swap;
                swap(mError, other.mError);
            }
            // Both value: nothing
        }
        else
        {
            if (has_value_)
            {
                new (&mError) E(std::move(other.mError));
                if constexpr (!std::is_trivially_destructible_v<E>)
                {
                    other.mError.~E();
                }
                has_value_ = false;
                other.has_value_ = true;
            }
            else
            {
                new (&other.mError) E(std::move(mError));
                if constexpr (!std::is_trivially_destructible_v<E>)
                {
                    mError.~E();
                }
                has_value_ = true;
                other.has_value_ = false;
            }
        }
    }
};

#ifdef USE_VARIANT_STORAGE
/**
 * @struct VariantStorage<void, E>
 * @brief Specialized VariantStorage for void success type.
 * @tparam E Error type.
 */
template <typename E>
struct VariantStorage<void, E>
{
    using Unexpected = unexpected<E>;
    std::variant<std::monostate, Unexpected> data_; ///< monostate for void success

    VariantStorage() = default;

    void store_value()
    {
        data_.template emplace<std::monostate>();
    }

    template <typename... Args>
    void store_error(Args&&... args)
    {
        data_.template emplace<Unexpected>(std::forward<Args>(args)...);
    }

    template <typename Arg>
    void assign_error(Arg&& arg)
    {
        assert(data_.index() == 1);
        std::get<Unexpected>(data_).mError = std::forward<Arg>(arg);
    }

    constexpr bool has_value() const noexcept
    {
        return data_.index() == 0;
    }

    // No get_value

    E& get_error() &
    {
        assert(data_.index() == 1);
        return std::get<Unexpected>(data_).mError;
    }
    const E& get_error() const&
    {
        assert(data_.index() == 1);
        return std::get<Unexpected>(data_).mError;
    }
    E&& get_error() &&
    {
        assert(data_.index() == 1);
        return std::get<Unexpected>(std::move(data_)).mError;
    }
    const E&& get_error() const&&
    {
        assert(data_.index() == 1);
        return std::get<Unexpected>(std::move(data_)).mError;
    }

    void swap(VariantStorage& other) noexcept(std::is_nothrow_swappable_v<decltype(data_)>)
    {
        data_.swap(other.data_);
    }
};
#endif

/**
 * @class ExpectedImpl<void, E>
 * @brief Specialization for void success type (no value on success).
 * @tparam E Error type.
 * @tparam StoragePolicy Policy for storage.
 */
template <typename E, template <typename, typename> class StoragePolicy>
class [[nodiscard]] ExpectedImpl<void, E, StoragePolicy>
{
private:
    StoragePolicy<void, E> mStorage;

    static_assert(std::is_destructible_v<E>, "E must be destructible");
    static_assert(!std::is_reference_v<E> && !std::is_array_v<E> && !std::is_function_v<E>,
                  "E must not be reference, array, or function");
    static_assert(!std::is_same_v<E, std::in_place_t> && !std::is_same_v<E, unexpect_tag_t>,
                  "E must not be in_place_t or unexpect_tag_t");
    static_assert(!std::is_void_v<E>, "E must not be void");

public:
    using value_type = void;
    using error_type = E;
    using unexpected_type = unexpected<E>;

    template <typename U>
    using rebind = ExpectedImpl<U, E, StoragePolicy>;

    // --- Constructors ---

    constexpr ExpectedImpl() noexcept
        : mStorage()
    {
    }

    constexpr explicit ExpectedImpl(std::in_place_t) noexcept
    {
    }

    template <typename... Args>
    constexpr explicit ExpectedImpl(unexpect_tag_t,
                                    Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
    {
        mStorage.store_error(std::forward<Args>(args)...);
    }

    template <typename U, typename... Args>
    constexpr explicit ExpectedImpl(unexpect_tag_t, std::initializer_list<U> il, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<E, std::initializer_list<U>&, Args...>)
    {
        mStorage.store_error(il, std::forward<Args>(args)...);
    }

    template <typename G>
    constexpr ExpectedImpl(const unexpected<G>& ue) noexcept(std::is_nothrow_constructible_v<E, const G&>)
    {
        mStorage.store_error(ue.value());
    }

    template <typename G>
    constexpr ExpectedImpl(unexpected<G>&& ue) noexcept(std::is_nothrow_constructible_v<E, G&&>)
    {
        mStorage.store_error(std::move(ue).value());
    }

    ExpectedImpl(const ExpectedImpl& other) noexcept(std::is_nothrow_copy_constructible_v<E>)
    {
        if (!other.has_value())
        {
            mStorage.store_error(other.mStorage.get_error());
        }
    }

    ExpectedImpl(ExpectedImpl&& other) noexcept(std::is_nothrow_move_constructible_v<E>)
    {
        if (!other.has_value())
        {
            mStorage.store_error(std::move(other.mStorage.get_error()));
        }
    }

    ExpectedImpl& operator=(const ExpectedImpl& other) noexcept(std::is_nothrow_copy_constructible_v<E> &&
                                                                std::is_nothrow_copy_assignable_v<E>)
    {
        if (this != &other)
        {
            const bool this_has_val = has_value();
            const bool other_has_val = other.has_value();

            if (!this_has_val && !other_has_val)
            {
                // Both errors - direct assignment
                mStorage.assign_error(other.mStorage.get_error());
            }
            else if (!other_has_val)
            {
                // this is value, other is error
                mStorage.store_error(other.mStorage.get_error());
            }
            else if (!this_has_val)
            {
                // this is error, other is value
                mStorage.store_value();
            }
            // Both values: nothing to do
        }
        return *this;
    }

    ExpectedImpl& operator=(ExpectedImpl&& other) noexcept(std::is_nothrow_move_constructible_v<E> &&
                                                           std::is_nothrow_move_assignable_v<E>)
    {
        if (this != &other)
        {
            const bool this_has_val = has_value();
            const bool other_has_val = other.has_value();

            if (!this_has_val && !other_has_val)
            {
                // Both errors - direct assignment
                mStorage.assign_error(std::move(other.mStorage.get_error()));
            }
            else if (!other_has_val)
            {
                // this is value, other is error
                mStorage.store_error(std::move(other.mStorage.get_error()));
            }
            else if (!this_has_val)
            {
                // this is error, other is value
                mStorage.store_value();
            }
            // Both values: nothing to do
        }
        return *this;
    }

    template <typename G>
    ExpectedImpl& operator=(const unexpected<G>& ue) noexcept(std::is_nothrow_constructible_v<E, const G&> &&
                                                              std::is_nothrow_assignable_v<E&, const G&>)
    {
        if (has_value())
        {
            mStorage.store_error(ue.value());
        }
        else
        {
            mStorage.assign_error(ue.value());
        }
        return *this;
    }

    template <typename G>
    ExpectedImpl& operator=(unexpected<G>&& ue) noexcept(std::is_nothrow_constructible_v<E, G&&> &&
                                                         std::is_nothrow_assignable_v<E&, G&&>)
    {
        if (has_value())
        {
            mStorage.store_error(std::move(ue).value());
        }
        else
        {
            mStorage.assign_error(std::move(ue).value());
        }
        return *this;
    }

    // --- Emplace ---

    void emplace() noexcept
    {
        mStorage.store_value();
    }

    // --- Swap ---

    void swap(ExpectedImpl& other) noexcept(noexcept(mStorage.swap(other.mStorage)))
    {
        mStorage.swap(other.mStorage);
    }

    // --- Observers ---

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return mStorage.has_value();
    }

    [[nodiscard]] constexpr bool has_error() const noexcept
    {
        return !mStorage.has_value();
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    constexpr void operator*() const noexcept
    {
        assert(has_value());
    }

    constexpr void value() const
    {
        if (!has_value())
        {
            FATP_EXPECTED_THROW(bad_expected_access<E>(mStorage.get_error()));
        }
    }

    constexpr E& error() & noexcept
    {
        assert(!has_value());
        return mStorage.get_error();
    }
    constexpr const E& error() const& noexcept
    {
        assert(!has_value());
        return mStorage.get_error();
    }
    constexpr E&& error() && noexcept
    {
        assert(!has_value());
        return std::move(mStorage.get_error());
    }
    constexpr const E&& error() const&& noexcept
    {
        assert(!has_value());
        return std::move(mStorage.get_error());
    }

    template <typename G>
    [[nodiscard]] constexpr E error_or(G&& default_error) const&
    {
        return has_value() ? static_cast<E>(std::forward<G>(default_error)) : mStorage.get_error();
    }

    template <typename G>
    [[nodiscard]] constexpr E error_or(G&& default_error) &&
    {
        return has_value() ? static_cast<E>(std::forward<G>(default_error)) : std::move(mStorage.get_error());
    }

    template <typename F>
    [[nodiscard]] constexpr E error_or_else(F&& f) const&
    {
        return has_value() ? std::forward<F>(f)() : mStorage.get_error();
    }

    template <typename F>
    [[nodiscard]] constexpr E error_or_else(F&& f) &&
    {
        return has_value() ? std::forward<F>(f)() : std::move(mStorage.get_error());
    }

    // --- Monadic Functions (Success Path) ---

    template <typename F>
    [[nodiscard]] constexpr auto map(F&& f) &
    {
        using U = std::remove_cvref_t<std::invoke_result_t<F>>;
        if (has_value())
        {
            if constexpr (std::is_void_v<U>)
            {
                std::invoke(std::forward<F>(f));
                return ExpectedImpl<void, E, StoragePolicy>();
            }
            else
            {
                return ExpectedImpl<U, E, StoragePolicy>(std::in_place, std::invoke(std::forward<F>(f)));
            }
        }
        return ExpectedImpl<U, E, StoragePolicy>(unexpect, mStorage.get_error());
    }

    template <typename F>
    [[nodiscard]] constexpr auto map(F&& f) const&
    {
        using U = std::remove_cvref_t<std::invoke_result_t<F>>;
        if (has_value())
        {
            if constexpr (std::is_void_v<U>)
            {
                std::invoke(std::forward<F>(f));
                return ExpectedImpl<void, E, StoragePolicy>();
            }
            else
            {
                return ExpectedImpl<U, E, StoragePolicy>(std::in_place, std::invoke(std::forward<F>(f)));
            }
        }
        return ExpectedImpl<U, E, StoragePolicy>(unexpect, mStorage.get_error());
    }

    template <typename F>
    [[nodiscard]] constexpr auto map(F&& f) &&
    {
        using U = std::remove_cvref_t<std::invoke_result_t<F>>;
        if (has_value())
        {
            if constexpr (std::is_void_v<U>)
            {
                std::invoke(std::forward<F>(f));
                return ExpectedImpl<void, E, StoragePolicy>();
            }
            else
            {
                return ExpectedImpl<U, E, StoragePolicy>(std::in_place, std::invoke(std::forward<F>(f)));
            }
        }
        return ExpectedImpl<U, E, StoragePolicy>(unexpect, std::move(mStorage.get_error()));
    }

    template <typename F>
    [[nodiscard]] constexpr auto map(F&& f) const&&
    {
        using U = std::remove_cvref_t<std::invoke_result_t<F>>;
        if (has_value())
        {
            if constexpr (std::is_void_v<U>)
            {
                std::invoke(std::forward<F>(f));
                return ExpectedImpl<void, E, StoragePolicy>();
            }
            else
            {
                return ExpectedImpl<U, E, StoragePolicy>(std::in_place, std::invoke(std::forward<F>(f)));
            }
        }
        return ExpectedImpl<U, E, StoragePolicy>(unexpect, std::move(mStorage.get_error()));
    }

    template <typename F>
    [[nodiscard]] constexpr auto transform(F&& f) &
    {
        return map(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto transform(F&& f) const&
    {
        return map(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto transform(F&& f) &&
    {
        return std::move(*this).map(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto transform(F&& f) const&&
    {
        return std::move(*this).map(std::forward<F>(f));
    }

    template <typename F>
    [[nodiscard]] constexpr auto and_then(F&& f) &
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F>>;
        static_assert(expected_compatible<Result, E>, "and_then must return ExpectedImpl<U, E> with same E");
        if (has_value())
        {
            return std::invoke(std::forward<F>(f));
        }
        return Result(unexpect, mStorage.get_error());
    }

    template <typename F>
    [[nodiscard]] constexpr auto and_then(F&& f) const&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F>>;
        static_assert(expected_compatible<Result, E>, "and_then must return ExpectedImpl<U, E> with same E");
        if (has_value())
        {
            return std::invoke(std::forward<F>(f));
        }
        return Result(unexpect, mStorage.get_error());
    }

    template <typename F>
    [[nodiscard]] constexpr auto and_then(F&& f) &&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F>>;
        static_assert(expected_compatible<Result, E>, "and_then must return ExpectedImpl<U, E> with same E");
        if (has_value())
        {
            return std::invoke(std::forward<F>(f));
        }
        return Result(unexpect, std::move(mStorage.get_error()));
    }

    template <typename F>
    [[nodiscard]] constexpr auto and_then(F&& f) const&&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F>>;
        static_assert(expected_compatible<Result, E>, "and_then must return ExpectedImpl<U, E> with same E");
        if (has_value())
        {
            return std::invoke(std::forward<F>(f));
        }
        return Result(unexpect, std::move(mStorage.get_error()));
    }

    template <typename F>
    [[nodiscard]] constexpr auto flat_map(F&& f) &
    {
        return and_then(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto flat_map(F&& f) const&
    {
        return and_then(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto flat_map(F&& f) &&
    {
        return std::move(*this).and_then(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto flat_map(F&& f) const&&
    {
        return std::move(*this).and_then(std::forward<F>(f));
    }

    // --- Monadic Functions (Error Path) ---

    template <typename F>
    [[nodiscard]] constexpr auto map_error(F&& f) &
    {
        using G = std::remove_cvref_t<std::invoke_result_t<F, E&>>;
        if (!has_value())
        {
            return ExpectedImpl<void, G, StoragePolicy>(unexpect,
                                                        std::invoke(std::forward<F>(f), mStorage.get_error()));
        }
        return ExpectedImpl<void, G, StoragePolicy>();
    }

    template <typename F>
    [[nodiscard]] constexpr auto map_error(F&& f) const&
    {
        using G = std::remove_cvref_t<std::invoke_result_t<F, const E&>>;
        if (!has_value())
        {
            return ExpectedImpl<void, G, StoragePolicy>(unexpect,
                                                        std::invoke(std::forward<F>(f), mStorage.get_error()));
        }
        return ExpectedImpl<void, G, StoragePolicy>();
    }

    template <typename F>
    [[nodiscard]] constexpr auto map_error(F&& f) &&
    {
        using G = std::remove_cvref_t<std::invoke_result_t<F, E&&>>;
        if (!has_value())
        {
            return ExpectedImpl<void, G, StoragePolicy>(
                unexpect,
                std::invoke(std::forward<F>(f), std::move(mStorage.get_error())));
        }
        return ExpectedImpl<void, G, StoragePolicy>();
    }

    template <typename F>
    [[nodiscard]] constexpr auto map_error(F&& f) const&&
    {
        using G = std::remove_cvref_t<std::invoke_result_t<F, const E&&>>;
        if (!has_value())
        {
            return ExpectedImpl<void, G, StoragePolicy>(
                unexpect,
                std::invoke(std::forward<F>(f), std::move(mStorage.get_error())));
        }
        return ExpectedImpl<void, G, StoragePolicy>();
    }

    template <typename F>
    [[nodiscard]] constexpr auto transform_error(F&& f) &
    {
        return map_error(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto transform_error(F&& f) const&
    {
        return map_error(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto transform_error(F&& f) &&
    {
        return std::move(*this).map_error(std::forward<F>(f));
    }
    template <typename F>
    [[nodiscard]] constexpr auto transform_error(F&& f) const&&
    {
        return std::move(*this).map_error(std::forward<F>(f));
    }

    template <typename F>
    [[nodiscard]] constexpr auto or_else(F&& f) &
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, E&>>;
        static_assert(expected_with_value<Result, void>, "or_else must return ExpectedImpl<void, G>");
        if (!has_value())
        {
            return std::invoke(std::forward<F>(f), mStorage.get_error());
        }
        return Result();
    }

    template <typename F>
    [[nodiscard]] constexpr auto or_else(F&& f) const&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, const E&>>;
        static_assert(expected_with_value<Result, void>, "or_else must return ExpectedImpl<void, G>");
        if (!has_value())
        {
            return std::invoke(std::forward<F>(f), mStorage.get_error());
        }
        return Result();
    }

    template <typename F>
    [[nodiscard]] constexpr auto or_else(F&& f) &&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, E&&>>;
        static_assert(expected_with_value<Result, void>, "or_else must return ExpectedImpl<void, G>");
        if (!has_value())
        {
            return std::invoke(std::forward<F>(f), std::move(mStorage.get_error()));
        }
        return Result();
    }

    template <typename F>
    [[nodiscard]] constexpr auto or_else(F&& f) const&&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, const E&&>>;
        static_assert(expected_with_value<Result, void>, "or_else must return ExpectedImpl<void, G>");
        if (!has_value())
        {
            return std::invoke(std::forward<F>(f), std::move(mStorage.get_error()));
        }
        return Result();
    }

    // --- Inspection ---

    template <typename F>
    constexpr const ExpectedImpl& inspect(F&& f) const&
    {
        if (has_value())
        {
            std::invoke(std::forward<F>(f));
        }
        return *this;
    }

    template <typename F>
    constexpr ExpectedImpl& inspect(F&& f) &
    {
        if (has_value())
        {
            std::invoke(std::forward<F>(f));
        }
        return *this;
    }

    template <typename F>
    constexpr const ExpectedImpl& inspect_error(F&& f) const&
    {
        if (!has_value())
        {
            std::invoke(std::forward<F>(f), mStorage.get_error());
        }
        return *this;
    }

    template <typename F>
    constexpr ExpectedImpl& inspect_error(F&& f) &
    {
        if (!has_value())
        {
            std::invoke(std::forward<F>(f), mStorage.get_error());
        }
        return *this;
    }

    // --- Fold/Match ---

    template <typename ValF, typename ErrF>
    [[nodiscard]] constexpr auto fold(ValF&& val_f, ErrF&& err_f) const&
    {
        if (has_value())
        {
            return std::invoke(std::forward<ValF>(val_f));
        }
        return std::invoke(std::forward<ErrF>(err_f), mStorage.get_error());
    }

    template <typename ValF, typename ErrF>
    [[nodiscard]] constexpr auto fold(ValF&& val_f, ErrF&& err_f) &
    {
        if (has_value())
        {
            return std::invoke(std::forward<ValF>(val_f));
        }
        return std::invoke(std::forward<ErrF>(err_f), mStorage.get_error());
    }

    template <typename ValF, typename ErrF>
    [[nodiscard]] constexpr auto fold(ValF&& val_f, ErrF&& err_f) &&
    {
        if (has_value())
        {
            return std::invoke(std::forward<ValF>(val_f));
        }
        return std::invoke(std::forward<ErrF>(err_f), std::move(mStorage.get_error()));
    }

    template <typename ValF, typename ErrF>
    [[nodiscard]] constexpr auto fold(ValF&& val_f, ErrF&& err_f) const&&
    {
        if (has_value())
        {
            return std::invoke(std::forward<ValF>(val_f));
        }
        return std::invoke(std::forward<ErrF>(err_f), std::move(mStorage.get_error()));
    }
};

// --- Comparison Operators for void ---

template <typename E1, typename E2, template <typename, typename> class SP1, template <typename, typename> class SP2>
[[nodiscard]] constexpr bool operator==(const ExpectedImpl<void, E1, SP1>& lhs, const ExpectedImpl<void, E2, SP2>& rhs)
{
    if (lhs.has_value() != rhs.has_value())
    {
        return false;
    }
    if (lhs.has_value())
    {
        return true;
    }
    return lhs.error() == rhs.error();
}


template <typename E1, typename E2, template <typename, typename> class SP>
[[nodiscard]] constexpr bool operator==(const ExpectedImpl<void, E1, SP>& lhs, const unexpected<E2>& rhs)
{
    return !lhs.has_value() && lhs.error() == rhs.value();
}

template <typename E1, typename E2, template <typename, typename> class SP>
[[nodiscard]] constexpr bool operator==(const unexpected<E1>& lhs, const ExpectedImpl<void, E2, SP>& rhs)
{
    return !rhs.has_value() && lhs.value() == rhs.error();
}



// --- User-Facing Aliases ---

// --- Storage Policy Configuration ---

/**
 * @brief Configurable default storage policy.
 *
 * Users can define their own default storage by defining
 * FATP_DEFAULT_STORAGE before including this header.
 *
 * @example Use custom storage globally:
 * @code
 * #define FATP_DEFAULT_STORAGE ArenaStorage
 * #include "Expected.h"
 *
 * Expected<int> x = ...;  // Uses ArenaStorage<int, std::string>
 * @endcode
 */
#ifndef FATP_DEFAULT_STORAGE
#ifdef USE_VARIANT_STORAGE
#define FATP_DEFAULT_STORAGE VariantStorage
#else
#define FATP_DEFAULT_STORAGE UnionStorage
#endif
#endif

// =========================================================================
// TrivialStorage Specialization (ABI Optimization)
// =========================================================================

/**
 * @class ExpectedImpl<T, E, TrivialStorage>
 * @brief Specialization of ExpectedImpl for TrivialStorage to enable register passing.
 *
 * This specialization uses =default for all special member functions, making the
 * entire type trivially copyable. This allows the compiler to pass the object in
 * CPU registers (Itanium ABI) rather than by stack pointer.
 *
 * @tparam T Value type (must be trivially copyable)
 * @tparam E Error type (must be trivially copyable)
 *
 * @note Use TrivialExpected<T, E> alias for convenient access.
 *
 * Performance characteristics:
 * - sizeof(TrivialExpected<int, int>) = 8 bytes (fits in single register)
 * - Passed in registers on x64 (RAX for returns, RDI/RSI for args)
 * - No heap allocation, no virtual dispatch
 */
template <typename T, typename E>
class [[nodiscard]] ExpectedImpl<T, E, TrivialStorage>
{
    // Note: T == E is allowed (e.g., TrivialExpected<int, int> for register passing)
    static_assert(!std::is_void_v<T>, "Use ExpectedImpl<void, E> for void success");

private:
    TrivialStorage<T, E> mStorage;

public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = unexpected<E>;
    template <typename U>
    using rebind = ExpectedImpl<U, E, TrivialStorage>;
    struct uninitialized_tag
    {
    };

    // --- Trivial Special Members (CRITICAL for ABI) ---
    constexpr ExpectedImpl() = default;
    constexpr ExpectedImpl(const ExpectedImpl&) = default;
    constexpr ExpectedImpl(ExpectedImpl&&) = default;
    constexpr ExpectedImpl& operator=(const ExpectedImpl&) = default;
    constexpr ExpectedImpl& operator=(ExpectedImpl&&) = default;
    ~ExpectedImpl() = default;

    // --- Constructors ---

    constexpr ExpectedImpl(uninitialized_tag)
        : mStorage()
    {
    }

    template <typename U = T>
        requires (std::constructible_from<T, U&&> && !std::same_as<std::remove_cvref_t<U>, ExpectedImpl> &&
                  !std::same_as<std::remove_cvref_t<U>, std::in_place_t> &&
                  !std::same_as<std::remove_cvref_t<U>, unexpect_tag_t>)
    constexpr ExpectedImpl(U&& v)
        : mStorage(std::in_place, std::forward<U>(v))
    {
    }

    template <typename... Args>
        requires std::constructible_from<T, Args...>
    constexpr explicit ExpectedImpl(std::in_place_t, Args&&... args)
        : mStorage(std::in_place, std::forward<Args>(args)...)
    {
    }

    template <typename... Args>
        requires std::constructible_from<E, Args...>
    constexpr explicit ExpectedImpl(unexpect_tag_t, Args&&... args)
        : mStorage(unexpect, std::forward<Args>(args)...)
    {
    }

    template <typename G>
        requires std::constructible_from<E, const G&>
    constexpr ExpectedImpl(const unexpected<G>& ue)
        : mStorage(unexpect, ue.value())
    {
    }

    template <typename G>
        requires std::constructible_from<E, G&&>
    constexpr ExpectedImpl(unexpected<G>&& ue)
        : mStorage(unexpect, std::move(ue).value())
    {
    }

    // --- Observers ---

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return mStorage.has_value();
    }
    [[nodiscard]] constexpr bool has_error() const noexcept
    {
        return !mStorage.has_value();
    }
    [[nodiscard]] constexpr bool is_initialized() const noexcept
    {
        return true;
    }
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    // --- Unchecked Accessors ---

    constexpr T* operator->() noexcept
    {
        return &mStorage.get_value();
    }
    constexpr const T* operator->() const noexcept
    {
        return &mStorage.get_value();
    }

    constexpr T& operator*() & noexcept
    {
        return mStorage.get_value();
    }
    constexpr const T& operator*() const& noexcept
    {
        return mStorage.get_value();
    }
    constexpr T&& operator*() && noexcept
    {
        return std::move(mStorage.get_value());
    }
    constexpr const T&& operator*() const&& noexcept
    {
        return std::move(mStorage.get_value());
    }

    // --- Unchecked Value Accessors (for verified hot paths) ---

    [[nodiscard]] constexpr T& value_unchecked() & noexcept
    {
        return mStorage.get_value();
    }
    [[nodiscard]] constexpr const T& value_unchecked() const& noexcept
    {
        return mStorage.get_value();
    }
    [[nodiscard]] constexpr T&& value_unchecked() && noexcept
    {
        return std::move(mStorage.get_value());
    }
    [[nodiscard]] constexpr const T&& value_unchecked() const&& noexcept
    {
        return std::move(mStorage.get_value());
    }

    // --- Throwing Value Accessors ---

    [[nodiscard]] constexpr T& value() &
    {
        if (!has_value())
        {
            FATP_EXPECTED_THROW(bad_expected_access<E>(mStorage.get_error()));
        }
        return mStorage.get_value();
    }
    [[nodiscard]] constexpr const T& value() const&
    {
        if (!has_value())
        {
            FATP_EXPECTED_THROW(bad_expected_access<E>(mStorage.get_error()));
        }
        return mStorage.get_value();
    }
    [[nodiscard]] constexpr T&& value() &&
    {
        if (!has_value())
        {
            FATP_EXPECTED_THROW(bad_expected_access<E>(std::move(mStorage.get_error())));
        }
        return std::move(mStorage.get_value());
    }
    [[nodiscard]] constexpr const T&& value() const&&
    {
        if (!has_value())
        {
            FATP_EXPECTED_THROW(bad_expected_access<E>(mStorage.get_error()));
        }
        return std::move(mStorage.get_value());
    }

    // --- Error Accessors ---

    constexpr E& error() & noexcept
    {
        return mStorage.get_error();
    }
    constexpr const E& error() const& noexcept
    {
        return mStorage.get_error();
    }
    constexpr E&& error() && noexcept
    {
        return std::move(mStorage.get_error());
    }
    constexpr const E&& error() const&& noexcept
    {
        return std::move(mStorage.get_error());
    }

    // --- value_or / error_or ---

    template <typename U>
    [[nodiscard]] constexpr T value_or(U&& default_val) const&
    {
        return has_value() ? mStorage.get_value() : static_cast<T>(std::forward<U>(default_val));
    }

    template <typename U>
    [[nodiscard]] constexpr T value_or(U&& default_val) &&
    {
        return has_value() ? std::move(mStorage.get_value()) : static_cast<T>(std::forward<U>(default_val));
    }

    template <typename U>
    [[nodiscard]] constexpr E error_or(U&& default_err) const&
    {
        return has_value() ? static_cast<E>(std::forward<U>(default_err)) : mStorage.get_error();
    }

    // --- Monadic Operations ---

    template <typename F>
    [[nodiscard]] constexpr auto map(F&& f) const&
    {
        using U = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
        if (has_value())
        {
            if constexpr (std::is_void_v<U>)
            {
                std::invoke(std::forward<F>(f), mStorage.get_value());
                return ExpectedImpl<void, E, UnionStorage>();
            }
            else
            {
                return ExpectedImpl<U, E, TrivialStorage>(std::in_place,
                                                          std::invoke(std::forward<F>(f), mStorage.get_value()));
            }
        }
        return ExpectedImpl<U, E, TrivialStorage>(unexpect, mStorage.get_error());
    }

    template <typename F>
    [[nodiscard]] constexpr auto and_then(F&& f) const&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
        if (has_value())
        {
            return std::invoke(std::forward<F>(f), mStorage.get_value());
        }
        return Result(unexpect, mStorage.get_error());
    }

    template <typename F>
    [[nodiscard]] constexpr auto or_else(F&& f) const&
    {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, const E&>>;
        if (has_value())
        {
            return Result(std::in_place, mStorage.get_value());
        }
        return std::invoke(std::forward<F>(f), mStorage.get_error());
    }

    template <typename F>
    [[nodiscard]] constexpr auto transform_error(F&& f) const&
    {
        using G = std::remove_cvref_t<std::invoke_result_t<F, const E&>>;
        if (has_value())
        {
            return ExpectedImpl<T, G, TrivialStorage>(std::in_place, mStorage.get_value());
        }
        return ExpectedImpl<T, G, TrivialStorage>(unexpect, std::invoke(std::forward<F>(f), mStorage.get_error()));
    }

    // --- Swap ---

    void swap(ExpectedImpl& other) noexcept
    {
        mStorage.swap(other.mStorage);
    }

    // --- Emplace ---

    template <typename... Args>
    T& emplace(Args&&... args)
    {
        mStorage.store_value(std::forward<Args>(args)...);
        return mStorage.get_value();
    }

};

// --- User-Facing Aliases ---

/**
 * @brief Primary Expected type with configurable storage.
 *
 * Storage policy is controlled by:
 * 1. FATP_DEFAULT_STORAGE macro (for custom policies)
 * 2. USE_VARIANT_STORAGE macro (for built-in VariantStorage)
 * 3. Default: UnionStorage
 *
 * @tparam T Value type
 * @tparam E Error type (default: std::string)
 *
 * @example Basic usage:
 * @code
 * Expected<int> divide(int a, int b) {
 *     if (b == 0) return unexpected{"division by zero"};
 *     return a / b;
 * }
 * @endcode
 *
 * @example Custom storage policy:
 * @code
 * // In your build system or before #include:
 * #define FATP_DEFAULT_STORAGE ArenaStorage
 * #include "Expected.h"
 *
 * Expected<int> x = ...;           // ArenaStorage<int, std::string>
 * Expected<Config> y = ...;        // ArenaStorage<Config, std::string>
 * Expected<int, Error> z = ...;    // ArenaStorage<int, Error>
 * @endcode
 */
template <typename T, typename E = std::string>
using Expected = ExpectedImpl<T, E, FATP_DEFAULT_STORAGE>;

/**
 * @brief Convenience alias for Expected with std::string error type.
 * @tparam T Value type
 */
template <typename T>
using Result = Expected<T, std::string>;

/**
 * @brief Convenience alias for void Expected (status-only operations).
 */
using Status = Expected<void, std::string>;

/**
 * @brief Zero-overhead Expected for trivially copyable types.
 *
 * Use this alias when maximum HPC performance is required and T/E are
 * trivially copyable. Unlike the default Expected, TrivialExpected can
 * be passed in CPU registers on x64 systems.
 *
 * @tparam T Value type (must be trivially copyable)
 * @tparam E Error type (must be trivially copyable, defaults to int)
 *
 * @code
 * // For hot paths with simple types
 * fat_p::TrivialExpected<int, int> fast_compute(int x) {
 *     if (x < 0) return fat_p::unexpected(-1);
 *     return x * 2;
 * }
 * @endcode
 */
template <typename T, typename E = int>
using TrivialExpected = ExpectedImpl<T, E, TrivialStorage>;

/**
 * @brief Factory function to create an Expected in value state.
 * @tparam E Error type (defaults to std::string)
 * @tparam T Value type (deduced from argument)
 * @param value The value to wrap
 * @return Expected<std::decay_t<T>, E> containing the value
 *
 * @code
 * auto result = make_expected<std::string>(42);  // Expected<int, std::string>
 * auto result2 = make_expected<MyError>(compute()); // Expected<ComputeResult, MyError>
 * @endcode
 */
template <typename E = std::string, typename T>
constexpr Expected<std::decay_t<T>, E> make_expected(T&& value)
{
    return Expected<std::decay_t<T>, E>(std::forward<T>(value));
}

/**
 * @brief Explicit alias for UnionStorage (bypasses configuration).
 * Use when you need guaranteed UnionStorage regardless of macro settings.
 */
template <typename T, typename E = std::string>
using ExpectedUnion = ExpectedImpl<T, E, UnionStorage>;

#ifdef USE_VARIANT_STORAGE
/**
 * @brief Explicit alias for VariantStorage (bypasses configuration).
 * Use when you need guaranteed VariantStorage regardless of macro settings.
 */
template <typename T, typename E = std::string>
using ExpectedVariant = ExpectedImpl<T, E, VariantStorage>;
#endif

// --- CTAD Deduction Guides ---

template <typename T>
ExpectedImpl(T) -> ExpectedImpl<T, std::string, UnionStorage>;

template <typename E>
ExpectedImpl(unexpected<E>) -> ExpectedImpl<void, E, UnionStorage>;

// --- Swap specialization ---

template <typename T, typename E, template <typename, typename> class SP>
void swap(ExpectedImpl<T, E, SP>& lhs, ExpectedImpl<T, E, SP>& rhs) noexcept(noexcept(lhs.swap(rhs)))
{
    lhs.swap(rhs);
}

template <typename E>
void swap(unexpected<E>& lhs, unexpected<E>& rhs) noexcept(noexcept(lhs.swap(rhs)))
{
    lhs.swap(rhs);
}


// =============================================================================
// Ordering Operators
// =============================================================================


// =============================================================================
// Three-Way Comparison (C++20)
// =============================================================================

/**
 * @brief Three-way comparison operator for Expected (C++20+)
 * @details Provides all six comparison operators (<, <=, >, >=, ==, !=) via single definition
 *
 * Ordering semantics:
 * - error < value (errors sort before values)
 * - Within same state: Compare contained objects using their <=>
 * - Returns strong_ordering if both T and E support it
 *
 * @note Requires that both T and E are three-way-comparable
 * @complexity O(1) - Single comparison operation
 *
 * @code
 * Expected<int, string> v1(42), v2(43), err(unexpected{"error"});
 *
 * auto cmp1 = v1 <=> v2;  // strong_ordering::less
 * auto cmp2 = v1 <=> v1;  // strong_ordering::equal
 * auto cmp3 = err <=> v1; // strong_ordering::less (error < value)
 *
 * // All six comparison operators work automatically:
 * bool b1 = (v1 < v2);   // true
 * bool b2 = (v1 <= v1);  // true
 * bool b3 = (v2 > v1);   // true
 * bool b4 = (v1 >= v1);  // true
 * bool b5 = (v1 == v1);  // true
 * bool b6 = (v1 != v2);  // true
 * @endcode
 */
template <typename T1, typename E1, typename T2, typename E2>
    constexpr auto operator<=>
    (const Expected<T1, E1>& lhs, const Expected<T2, E2>& rhs)
        requires std::three_way_comparable_with<T1, T2>&& std::three_way_comparable_with<E1, E2>
{
    // Error < Value ordering (matches C++23 std::expected)
    if (lhs.has_value() != rhs.has_value())
    {
        return lhs.has_value() ? std::strong_ordering::greater : std::strong_ordering::less;
    }

    // Both have value: compare values
    if (lhs.has_value())
    {
        return *lhs <=> *rhs;
    }

    // Both have error: compare errors
    return lhs.error() <=> rhs.error();
}

/**
 * @brief Three-way comparison for void Expected specialization (C++20+)
 */
template <typename E1, typename E2>
    constexpr auto operator<=>
    (const Expected<void, E1>& lhs, const Expected<void, E2>& rhs)requires std::three_way_comparable_with<E1, E2>
{
    // Error < Value ordering
    if (lhs.has_value() != rhs.has_value())
    {
        return lhs.has_value() ? std::strong_ordering::greater : std::strong_ordering::less;
    }

    // Both have value (void): equal
    if (lhs.has_value())
    {
        return std::strong_ordering::equal;
    }

    // Both have error: compare errors
    return lhs.error() <=> rhs.error();
}


// =============================================================================
// Integration with std::expected (C++23)
// =============================================================================

#if FATP_HAS_EXPECTED

/**
 * @section std_expected_integration Integration with std::expected
 *
 * When C++23 std::expected is available, this library provides conversion utilities
 * for interoperability. Use these when interfacing with code that requires std::expected,
 * while maintaining access to custom features (policies, inspect, etc.) in your code.
 *
 * **Available Conversions:**
 * - to_std_expected(): Convert to std::expected (loses storage policy)
 * - from_std_expected(): Convert from std::expected (uses default storage)
 *
 * **Use Cases:**
 * 1. Passing to libraries that require std::expected
 * 2. Testing behavior against standard implementation
 * 3. Gradual migration to C++23
 *
 * @code
 * // Example: Bridge between implementations
 * fat_p::Expected<int, string> my_result = compute();
 *
 * // Pass to third-party library expecting std::expected
 * std::expected<int, string> for_library = to_std_expected(my_result);
 * third_party_function(for_library);
 *
 * // Convert result back to use custom features
 * auto back = from_std_expected(library_result);
 * back.inspect([](int x) { log("Value:", x); });  // Custom feature
 * @endcode
 */

// --- Conversion: fat_p::Expected -> std::expected ---

/**
 * @brief Convert fat_p::Expected to std::expected (lvalue)
 * @tparam T Value type
 * @tparam E Error type
 * @param exp The Expected object to convert
 * @return std::expected<T, E> Standard Expected
 * @note Storage policy information is lost in conversion
 * @complexity O(1) copy construction
 */
template <typename T, typename E>
constexpr std::expected<T, E> to_std_expected(const Expected<T, E>& exp)
{
    if (exp.has_value())
    {
        return std::expected<T, E>(std::in_place, *exp);
    }
    else
    {
        return std::expected<T, E>(std::unexpect, exp.error());
    }
}

/**
 * @brief Convert fat_p::Expected to std::expected (rvalue)
 * @tparam T Value type
 * @tparam E Error type
 * @param exp The Expected object to convert (will be moved from)
 * @return std::expected<T, E> Standard Expected
 * @note Storage policy information is lost in conversion
 * @complexity O(1) move construction
 */
template <typename T, typename E>
constexpr std::expected<T, E> to_std_expected(Expected<T, E>&& exp)
{
    if (exp.has_value())
    {
        return std::expected<T, E>(std::in_place, std::move(*exp));
    }
    else
    {
        return std::expected<T, E>(std::unexpect, std::move(exp).error());
    }
}

/**
 * @brief Convert fat_p::Expected<void, E> to std::expected<void, E> (lvalue)
 */
template <typename E>
constexpr std::expected<void, E> to_std_expected(const Expected<void, E>& exp)
{
    if (exp.has_value())
    {
        return std::expected<void, E>();
    }
    else
    {
        return std::expected<void, E>(std::unexpect, exp.error());
    }
}

/**
 * @brief Convert fat_p::Expected<void, E> to std::expected<void, E> (rvalue)
 */
template <typename E>
constexpr std::expected<void, E> to_std_expected(Expected<void, E>&& exp)
{
    if (exp.has_value())
    {
        return std::expected<void, E>();
    }
    else
    {
        return std::expected<void, E>(std::unexpect, std::move(exp).error());
    }
}

// --- Conversion: std::expected -> fat_p::Expected ---

/**
 * @brief Convert std::expected to fat_p::Expected (lvalue)
 * @tparam T Value type
 * @tparam E Error type
 * @param exp The std::expected object to convert
 * @return Expected<T, E> Custom Expected with default storage policy
 * @complexity O(1) copy construction
 */
template <typename T, typename E>
constexpr Expected<T, E> from_std_expected(const std::expected<T, E>& exp)
{
    if (exp.has_value())
    {
        return Expected<T, E>(std::in_place, *exp);
    }
    else
    {
        return Expected<T, E>(unexpected{exp.error()});
    }
}

/**
 * @brief Convert std::expected to fat_p::Expected (rvalue)
 * @tparam T Value type
 * @tparam E Error type
 * @param exp The std::expected object to convert (will be moved from)
 * @return Expected<T, E> Custom Expected with default storage policy
 * @complexity O(1) move construction
 */
template <typename T, typename E>
constexpr Expected<T, E> from_std_expected(std::expected<T, E>&& exp)
{
    if (exp.has_value())
    {
        return Expected<T, E>(std::in_place, std::move(*exp));
    }
    else
    {
        return Expected<T, E>(unexpected{std::move(exp).error()});
    }
}

/**
 * @brief Convert std::expected<void, E> to fat_p::Expected<void, E> (lvalue)
 */
template <typename E>
constexpr Expected<void, E> from_std_expected(const std::expected<void, E>& exp)
{
    if (exp.has_value())
    {
        return Expected<void, E>();
    }
    else
    {
        return Expected<void, E>(unexpected{exp.error()});
    }
}

/**
 * @brief Convert std::expected<void, E> to fat_p::Expected<void, E> (rvalue)
 */
template <typename E>
constexpr Expected<void, E> from_std_expected(std::expected<void, E>&& exp)
{
    if (exp.has_value())
    {
        return Expected<void, E>();
    }
    else
    {
        return Expected<void, E>(unexpected{std::move(exp).error()});
    }
}

#endif // FATP_HAS_EXPECTED

// Specialize traits for std::expected compatibility (C++23)
#if FATP_HAS_EXPECTED
template <typename V, typename Err>
struct is_expected_compatible<std::expected<V, Err>, Err> : std::true_type
{
};

template <typename Val, typename Err>
struct is_expected_with_value<std::expected<Val, Err>, Val> : std::true_type
{
};

#endif

// --- FATP_EXPECTED_TRY Macro ---

#define FATP_EXPECTED_TRY_CONCAT_(a, b) a##b
#define FATP_EXPECTED_TRY_CONCAT(a, b) FATP_EXPECTED_TRY_CONCAT_(a, b)
#define FATP_EXPECTED_TRY_UNIQUE_NAME FATP_EXPECTED_TRY_CONCAT(expected_try_result_, __LINE__)

#define FATP_EXPECTED_TRY(var, expr)                                                \
    auto FATP_EXPECTED_TRY_UNIQUE_NAME = (expr);                                    \
    if (!FATP_EXPECTED_TRY_UNIQUE_NAME.has_value())                                 \
    {                                                                               \
        return fat_p::unexpected(std::move(FATP_EXPECTED_TRY_UNIQUE_NAME).error()); \
    }                                                                               \
    [[maybe_unused]] decltype(auto) var = std::move(FATP_EXPECTED_TRY_UNIQUE_NAME).value()

#define FATP_EXPECTED_TRY_VOID(expr)                                                    \
    do                                                                                  \
    {                                                                                   \
        auto FATP_EXPECTED_TRY_UNIQUE_NAME = (expr);                                    \
        if (!FATP_EXPECTED_TRY_UNIQUE_NAME.has_value())                                 \
        {                                                                               \
            return fat_p::unexpected(std::move(FATP_EXPECTED_TRY_UNIQUE_NAME).error()); \
        }                                                                               \
    } while (0)

/**
 * @brief Assigns result to existing variable or returns error (Google Abseil style).
 *
 * Similar to FATP_EXPECTED_TRY but assigns to an already-declared variable.
 * Useful when the variable needs to be declared before the assignment.
 *
 * @param lhs The variable to assign to (must already be declared)
 * @param expr Expression returning an Expected
 *
 * @code
 * int value;
 * FATP_EXPECTED_ASSIGN_OR_RETURN(value, compute_value());
 * // value now contains the result, or function returned early with error
 * @endcode
 */
#define FATP_EXPECTED_ASSIGN_OR_RETURN(lhs, expr)                                                              \
    do                                                                                                         \
    {                                                                                                          \
        auto FATP_EXPECTED_TRY_CONCAT(expected_assign_, __LINE__) = (expr);                                    \
        if (!FATP_EXPECTED_TRY_CONCAT(expected_assign_, __LINE__).has_value())                                 \
        {                                                                                                      \
            return fat_p::unexpected(std::move(FATP_EXPECTED_TRY_CONCAT(expected_assign_, __LINE__)).error()); \
        }                                                                                                      \
        lhs = std::move(FATP_EXPECTED_TRY_CONCAT(expected_assign_, __LINE__)).value();                         \
    } while (0)

// Note: FATP_EXPECTED_TRY_CONCAT_, FATP_EXPECTED_TRY_CONCAT, and
// FATP_EXPECTED_TRY_UNIQUE_NAME are intentionally NOT #undef'd because they are
// required for the user-facing macros FATP_EXPECTED_TRY, FATP_EXPECTED_TRY_VOID,
// and FATP_EXPECTED_ASSIGN_OR_RETURN to function correctly.

} // namespace fat_p

// --- std::hash specialization ---

namespace std
{

template <typename T, typename E, template <typename, typename> class SP>
struct hash<fat_p::ExpectedImpl<T, E, SP>>
{
    size_t operator()(const fat_p::ExpectedImpl<T, E, SP>& exp) const
        noexcept(noexcept(hash<T>{}(std::declval<const T&>())) && noexcept(hash<E>{}(std::declval<const E&>())))
    {
        if (exp.has_value())
        {
            return hash<T>{}(*exp);
        }
        else
        {
            return hash<E>{}(exp.error()) ^ 0x9e3779b9;
        }
    }
};

template <typename E, template <typename, typename> class SP>
struct hash<fat_p::ExpectedImpl<void, E, SP>>
{
    size_t operator()(const fat_p::ExpectedImpl<void, E, SP>& exp) const
        noexcept(noexcept(hash<E>{}(std::declval<const E&>())))
    {
        if (exp.has_value())
        {
            return 0;
        }
        else
        {
            return hash<E>{}(exp.error()) ^ 0x9e3779b9;
        }
    }
};

template <typename E>
struct hash<fat_p::unexpected<E>>
{
    size_t operator()(const fat_p::unexpected<E>& unexp) const noexcept(noexcept(hash<E>{}(unexp.value())))
    {
        return hash<E>{}(unexp.value());
    }
};

} // namespace std
