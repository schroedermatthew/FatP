/**
 * @file enforce_contextual_policies.h
 * @brief Defines type traits and policy tags essential for contextual contract
 * enforcement.
 *
 * @details The core component is the is_noexcept_function_ptr trait, which
 * detects the 'noexcept' specification on function and method pointers at
 * compile time. This information is used to select the correct Raiser
 * (throwing or non-throwing) via ContextualRaiserResolver.
 */
#pragma once
#include <type_traits>
#include <utility>

namespace cpp_utilities {

    struct NoThrowRaiser;

    // --- Policy Tags ---
    /**
     * @brief Policy tag selected when the function is marked 'noexcept'.
     * @details This policy directs the system to use NoThrowRaiser, adhering to
     * the function's exception safety guarantee.
     */
    struct NoexceptFunctionPolicy {};
    /**
     * @brief Policy tag selected when the function is capable of throwing.
     * @details This policy directs the system to use the default throwing
     * raiser (mapped from PredicateType or explicitly
     * specified) unless explicitly overridden.
     */
    struct ThrowingFunctionPolicy {};
    // --- Function Trait Checker (Detects noexcept in signature) ---
    /**
     * @brief Base trait for function pointer noexcept detection.
     * @tparam T The function or member function pointer type.
     * @details The primary template defaults to std::false_type.
     */
    template <typename T>
    struct is_noexcept_function_ptr : std::false_type {};
    // -----------------------------------------------------------------
    // Specializations for C-Style Function Pointers (R(*)(Args...))
    // -----------------------------------------------------------------
    /**
     * @brief Specialization for noexcept C-style function pointers.
     * @tparam R The return type.
     * @tparam Args The parameter types.
     */
    template <typename R, typename... Args>
    struct is_noexcept_function_ptr<R(*)(Args...) noexcept> : std::true_type {};
    // -----------------------------------------------------------------
    // Specializations for Member Function Pointers (R(C::*)(Args...) [Qualifiers])
    // -----------------------------------------------------------------
    /**
     * @brief Specialization for non-qualified, noexcept member function pointers.
     */
    template <typename R, typename C, typename... Args>
    struct is_noexcept_function_ptr<R(C::*)(Args...) noexcept> : std::true_type {};
    /**
     * @brief Specialization for const-qualified, noexcept member function pointers.
     */
    template <typename R, typename C, typename... Args>
    struct is_noexcept_function_ptr<R(C::*)(Args...) const noexcept>
        : std::true_type {
    };
    /**
     * @brief Specialization for volatile-qualified, noexcept member function
     * pointers.
     */
    template <typename R, typename C, typename... Args>
    struct is_noexcept_function_ptr<R(C::*)(Args...) volatile noexcept>
        : std::true_type {
    };
    /**
     * @brief Specialization for const volatile-qualified, noexcept member function
     * pointers.
     */
    template <typename R, typename C, typename... Args>
    struct is_noexcept_function_ptr<R(C::*)(Args...) const volatile noexcept>
        : std::true_type {
    };
    /**
     * @brief Specialization for lvalue reference-qualified, noexcept member
     * function pointers (trailing &).
     */
    template <typename R, typename C, typename... Args>
    struct is_noexcept_function_ptr<R(C::*)(Args...) & noexcept>
        : std::true_type {
    };
    /**
     * @brief Specialization for rvalue reference-qualified, noexcept member
     * function pointers (trailing &&).
     */
    template <typename R, typename C, typename... Args>
    struct is_noexcept_function_ptr<R(C::*)(Args...) && noexcept>
        : std::true_type {
    };
    /**
     * @brief Specialization for const lvalue reference-qualified, noexcept member
     * function pointers (const &).
     */
    template <typename R, typename C, typename... Args>
    struct is_noexcept_function_ptr<R(C::*)(Args...) const & noexcept>
        : std::true_type {
    };
    /**
     * @brief Specialization for const rvalue reference-qualified, noexcept member
     * function pointers (const &&).
     */
    template <typename R, typename C, typename... Args>
    struct is_noexcept_function_ptr<R(C::*)(Args...) const && noexcept>
        : std::true_type {
    };
    /**
     * @brief Specialization for volatile lvalue reference-qualified, noexcept member
     * function pointers (volatile &).
     */
    template <typename R, typename C, typename... Args>
    struct is_noexcept_function_ptr<R(C::*)(Args...) volatile & noexcept>
        : std::true_type {
    };
    /**
     * @brief Specialization for volatile rvalue reference-qualified, noexcept member
     * function pointers (volatile &&).
     */
    template <typename R, typename C, typename... Args>
    struct is_noexcept_function_ptr<R(C::*)(Args...) volatile && noexcept>
        : std::true_type {
    };
    /**
     * @brief Specialization for const volatile lvalue reference-qualified, noexcept
     * member function pointers (const volatile &).
     */
    template <typename R, typename C, typename... Args>
    struct is_noexcept_function_ptr<R(C::*)(Args...) const volatile & noexcept>
        : std::true_type {
    };
    /**
     * @brief Specialization for const volatile rvalue reference-qualified, noexcept
     * member function pointers (const volatile &&).
     */
    template <typename R, typename C, typename... Args>
    struct is_noexcept_function_ptr<R(C::*)(Args...) const volatile && noexcept>
        : std::true_type {
    };
    // --- Contextual Raiser Resolver (Updates: Extensible with more policies) ---
    template <typename NoexceptPolicy, typename ThrowingRaiser>
    struct ContextualRaiserResolver {
        using type = ThrowingRaiser;
    };
    template <typename ThrowingRaiser>
    struct ContextualRaiserResolver<NoexceptFunctionPolicy, ThrowingRaiser> {
        using type = NoThrowRaiser;
    };
    // Add custom resolver specializations if needed (extensibility)
} // namespace cpp_utilities