// DebugOnly.h
#ifndef CPP_UTILITIES_DEBUG_ONLY_H
#define CPP_UTILITIES_DEBUG_ONLY_H

#include <utility>

namespace cpp_utilities {

/**
 * @brief RAII wrapper for debug-only values that has zero overhead in release builds
 * @tparam T Type of value to store in debug builds
 * 
 * @details In debug builds (NDEBUG not defined), stores value normally.
 * In release builds, the class is empty and all operations are no-ops.
 * 
 * C++20 Enhancement: Uses [[no_unique_address]] for true zero-overhead in release
 * C++17 fallback: Empty class still takes 1 byte due to unique addressing rules
 * 
 * @note Performance: Zero overhead in release (assignment optimized to nothing)
 */
template <typename T>
struct DebugOnly {
#ifdef NDEBUG
    // In release, empty struct - minimal overhead
    // Note: Empty base optimization (EBO) would make this zero-size when used as base class,
    // but as a member, C++ requires 1 byte minimum for unique addressing.
    // Solution: Use [[no_unique_address]] attribute in C++20 for true zero-overhead.
    
    constexpr DebugOnly() noexcept = default;
    constexpr DebugOnly(const T&) noexcept {}  // No-op constructors
    constexpr DebugOnly(T&&) noexcept {}
    constexpr DebugOnly& operator=(const T&) noexcept { return *this; }
    constexpr DebugOnly& operator=(T&&) noexcept { return *this; }
    
    // Copy/move operations
    constexpr DebugOnly(const DebugOnly&) noexcept = default;
    constexpr DebugOnly(DebugOnly&&) noexcept = default;
    constexpr DebugOnly& operator=(const DebugOnly&) noexcept = default;
    constexpr DebugOnly& operator=(DebugOnly&&) noexcept = default;
    
    // No storage, so no accessors needed
#else
    T value;

    DebugOnly() : value() {}  // Value-initialize (zero for primitives, default constructor for classes)
    DebugOnly(const T& val) : value(val) {}
    DebugOnly(T&& val) : value(std::move(val)) {}
    DebugOnly& operator=(const T& val) { value = val; return *this; }
    DebugOnly& operator=(T&& val) { value = std::move(val); return *this; }

    // Implicit conversion operators for easy access
    operator T&() { return value; }
    operator const T&() const { return value; }
    
    // Explicit accessors
    T& get() { return value; }
    const T& get() const { return value; }
    
    T* operator->() { return &value; }
    const T* operator->() const { return &value; }
    
    T& operator*() { return value; }
    const T& operator*() const { return value; }
#endif
};

/**
 * @brief Usage example showing proper patterns:
 * 
 * @code
 * struct MyClass {
 *     int data;
 *     
 *     // C++20: Use [[no_unique_address]] for true zero-overhead
 *     #if __cplusplus >= 202002L
 *     [[no_unique_address]]
 *     #endif
 *     DebugOnly<std::string> debug_name;
 * };
 * 
 * // In C++20: sizeof(MyClass) == sizeof(int) in release
 * // In C++17: sizeof(MyClass) == sizeof(int) + 1 in release (still minimal)
 * @endcode
 */

}  // namespace cpp_utilities

#endif  // CPP_UTILITIES_DEBUG_ONLY_H
