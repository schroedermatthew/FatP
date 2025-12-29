/**
 * @file ValueGuard.h
 * @brief Provides the ValueGuard<T, Policy> class for temporary value
 * mutation with policy-driven restoration.
 *
 * @details This RAII utility temporarily assigns a new value to a target
 * variable and restores the original upon scope exit, with support for
 * custom restoration logic and exception safety. Policies control
 * restoration behavior (e.g., copy vs. move, conditional execution).
 *
 * Key features:
 * - Policy extensibility for custom restore (e.g., lambda-based or conditional)
 * - Early release for "keep mutation" scenarios
 * - State introspection (is_active, original access)
 * - Move semantics for efficient transfer in containers
 * - Deduction guides for seamless C++17 usage
 * - Support for move-only types with automatic detection
 * - Strong exception guarantee in constructors
 *
 * @note For strong exception safety, ensure Policy::is_nothrow_restore is true.
 * If restore can throw and an exception is already in flight, std::terminate 
 * will be called per C++ standard.
 *
 * @note Deduction guides disambiguate between CustomPolicy (2-arg callable) and
 * ConditionalPolicy (0-arg callable). For overloaded functors that satisfy both
 * interfaces, use the explicit factory functions: make_value_guard_custom() or
 * make_value_guard_conditional().
 *
 * @tparam T The type of the value being guarded (must be assignable).
 * @tparam Policy The restoration policy (defaults to ValueGuardCopyPolicy<T>).
 *
 * @version 2.4
 * @date 2025
 */
#pragma once

#include <utility>      // for std::move, std::forward
#include <type_traits>  // for type traits
#include <algorithm>    // for std::swap
#include <functional>   // for std::invoke

#include "FatPTypeTraits.h"

namespace fat_p {

// --- Policy Tags and Definitions ---

/**
 * @brief Policy for copy-based restoration.
 * @details Restores by copy-assignment: target = original.
 * Signature accepts T&& to match generic interface, but treats as lvalue.
 */
template <typename T>
struct ValueGuardCopyPolicy {
    static constexpr bool is_nothrow_restore = std::is_nothrow_copy_assignable_v<T>;
    
    void execute(T& target, T&& original) noexcept(is_nothrow_restore) {
        target = original;  // Copy, not move (original treated as lvalue)
    }
};

/**
 * @brief Policy for move-based restoration.
 * @details Restores by move-assignment: target = std::move(original).
 */
template <typename T>
struct ValueGuardMovePolicy {
    static constexpr bool is_nothrow_restore = std::is_nothrow_move_assignable_v<T>;
    
    void execute(T& target, T&& original) noexcept(is_nothrow_restore) {
        target = std::move(original);
    }
};

/**
 * @brief Policy that disables restoration entirely.
 * @details No action in destructor; equivalent to always calling release().
 */
template <typename T>
struct ValueGuardNoRestorePolicy {
    static constexpr bool is_nothrow_restore = true;
    
    void execute(T&, T&&) noexcept {}
};

/**
 * @brief Policy for conditional restoration.
 * @details Restores only if a user-provided condition evaluates to true.
 * The condition is evaluated in the destructor.
 *
 * @tparam Cond The type of the condition function (invocable as Cond() -> bool).
 */
template <typename T, typename Cond>
struct ValueGuardConditionalPolicy {
    static constexpr bool is_nothrow_restore =
        std::is_nothrow_invocable_r_v<bool, Cond> &&
        std::is_nothrow_move_assignable_v<T>;
    
    mutable Cond cond_;
    
    explicit ValueGuardConditionalPolicy(Cond&& cond) : cond_(std::move(cond)) {
        static_assert(std::is_invocable_r_v<bool, Cond>,
                      "Cond must be invocable returning bool");
    }
    
    void execute(T& target, T&& original) noexcept(is_nothrow_restore) {
        if (std::invoke(cond_)) {
            target = std::move(original);
        }
    }
};

/**
 * @brief Policy for custom restoration.
 * @details Restores using a user-provided function.
 *
 * @tparam F The type of the restorer function (invocable as F(T&, T&&)).
 */
template <typename T, typename F>
struct ValueGuardCustomPolicy {
    static constexpr bool is_nothrow_restore =
        noexcept(std::declval<F>()(std::declval<T&>(), std::declval<T&&>()));
    
    F restorer_;
    
    explicit ValueGuardCustomPolicy(F&& restorer) 
        : restorer_(std::forward<F>(restorer)) {
        static_assert(std::is_invocable_v<F, T&, T&&>,
                      "F must be invocable as F(T&, T&&)");
    }
    
    void execute(T& target, T&& original) noexcept(is_nothrow_restore) {
        std::invoke(restorer_, target, std::move(original));
    }
};

// --- Primary ValueGuard Class ---

/**
 * @brief RAII guard for temporary value mutation with automatic restoration.
 *
 * @tparam T The type of the value being guarded.
 * @tparam Policy The policy governing restoration behavior.
 */
template <typename T, typename Policy = ValueGuardCopyPolicy<T>>
class [[nodiscard]] ValueGuard : private Policy {
private:
    using OriginalType = T;

    // Static assertions for policy requirements
    static_assert(
        !std::is_same_v<Policy, ValueGuardMovePolicy<T>> ||
        (std::is_move_constructible_v<T> && std::is_move_assignable_v<T>),
        "T must be move-constructible and move-assignable for MovePolicy");
    
    static_assert(
        !std::is_same_v<Policy, ValueGuardCopyPolicy<T>> ||
        (std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>),
        "T must be copy-constructible and copy-assignable for CopyPolicy");

public:
    // --- Constructors (Strong Exception Guarantee) ---

    /**
     * @brief Constructs the guard with copy semantics for the new value.
     * @param target Mutable reference to the variable to guard.
     * @param new_value The temporary value to assign.
     */
    template <typename U = T, 
              std::enable_if_t<std::is_copy_constructible_v<U> && 
                               std::is_copy_assignable_v<U>, int> = 0>
    ValueGuard(T& target, const T& new_value)
        : Policy(), target_(&target), original_(target), active_(true)
    {
        try {
            *target_ = new_value;
        } catch (...) {
            *target_ = std::move(original_);
            throw;
        }
    }
    
    /**
     * @brief Constructs the guard with move semantics for the new value.
     * @param target Mutable reference to the variable to guard.
     * @param new_value Rvalue reference to the temporary value to assign.
     */
    ValueGuard(T& target, T&& new_value)
        : Policy(), 
          target_(&target), 
          original_(std::move(target)),
          active_(true)
    {
        try {
            *target_ = std::move(new_value);
        } catch (...) {
            *target_ = std::move(original_);
            throw;
        }
    }
    
    /**
     * @brief Constructs the guard with a custom restorer function (copy new_value).
     * @param target Mutable reference to the variable to guard.
     * @param new_value The temporary value to assign.
     * @param restorer The function to call for restoration.
     */
    template <typename F,
              typename = std::enable_if_t<std::is_invocable_v<F, T&, T&&>>>
    ValueGuard(T& target, const T& new_value, F&& restorer)
        : Policy(std::forward<F>(restorer)), 
          target_(&target), 
          original_(target), 
          active_(true)
    {
        try {
            *target_ = new_value;
        } catch (...) {
            *target_ = std::move(original_);
            throw;
        }
    }

    /**
     * @brief Constructs the guard with a custom restorer function (move new_value).
     * @param target Mutable reference to the variable to guard.
     * @param new_value Rvalue reference to the temporary value to assign.
     * @param restorer The function to call for restoration.
     */
    template <typename F,
              typename = std::enable_if_t<std::is_invocable_v<F, T&, T&&>>>
    ValueGuard(T& target, T&& new_value, F&& restorer)
        : Policy(std::forward<F>(restorer)), 
          target_(&target), 
          original_(std::move(target)), 
          active_(true)
    {
        try {
            *target_ = std::move(new_value);
        } catch (...) {
            *target_ = std::move(original_);
            throw;
        }
    }
    
    /**
     * @brief Constructs guard with move semantics and conditional policy.
     * @param target Mutable reference to the variable to guard.
     * @param new_value Rvalue reference to the temporary value to assign.
     * @param condition Condition to evaluate for restoration.
     */
    template <typename Cond,
              typename PolicyType = Policy,
              typename = std::enable_if_t<
                  std::is_same_v<PolicyType, ValueGuardConditionalPolicy<T, std::decay_t<Cond>>> &&
                  std::is_invocable_r_v<bool, Cond>>>
    ValueGuard(T& target, T&& new_value, Cond&& condition)
        : Policy(std::forward<Cond>(condition)),
          target_(&target),
          original_(std::move(target)),
          active_(true)
    {
        try {
            *target_ = std::move(new_value);
        } catch (...) {
            *target_ = std::move(original_);
            throw;
        }
    }
    
    // --- Move Semantics ---
    
    /**
     * @brief Move constructor. Transfers ownership from the source guard.
     * @param other The source guard to move from.
     */
    ValueGuard(ValueGuard&& other) noexcept(
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<Policy>)
        : Policy(std::move(static_cast<Policy&>(other))), 
          target_(other.target_), 
          original_(std::move(other.original_)),
          active_(other.active_)
    {
        other.active_ = false;
    }
    
    /**
     * @brief Move assignment operator. Transfers ownership from the source.
     * @details CRITICAL: Restores the current target before taking ownership
     * of the new one. This ensures no guarded value is silently abandoned.
     * @param other The source guard to move from.
     * @return Reference to this guard.
     */
    ValueGuard& operator=(ValueGuard&& other) noexcept(
        std::is_nothrow_move_assignable_v<Policy> &&
        Policy::is_nothrow_restore) 
    {
        if (this == &other) return *this;  // Explicit no-op for self-move
        
        // 1. Clean up current state if active
        if (active_) {
            this->execute(*target_, std::move(original_));
        }

        // 2. Transfer state from other
        static_cast<Policy&>(*this) = std::move(static_cast<Policy&>(other));
        target_ = other.target_;
        original_ = std::move(other.original_);
        active_ = other.active_;
        
        // 3. Deactivate other
        other.active_ = false;
        
        return *this;
    }
    
    // Delete copy operations to enforce unique ownership.
    ValueGuard(const ValueGuard&) = delete;
    ValueGuard& operator=(const ValueGuard&) = delete;
    
    // --- Destructor ---

    /**
     * @brief Restores the target to its original value if active.
     */
    ~ValueGuard() noexcept(Policy::is_nothrow_restore) {
        if (active_) {
            this->execute(*target_, std::move(original_));
        }
    }
    
    // --- API ---

    /**
     * @brief Disables automatic restoration, committing the mutation.
     */
    void release() noexcept { active_ = false; }
    
    /**
     * @brief Checks if the guard is still active (restoration pending).
     */
    [[nodiscard]] bool is_active() const noexcept { return active_; }
    
    /**
     * @brief Retrieves a const reference to the captured original value.
     */
    [[nodiscard]] const T& original() const noexcept { return original_; }
    
    /**
     * @brief Retrieves a reference to the current target value.
     */
    [[nodiscard]] T& current() noexcept { return *target_; }
    
    /**
     * @brief Retrieves a const reference to the current target value.
     */
    [[nodiscard]] const T& current() const noexcept { return *target_; }
    
    /**
     * @brief Swaps the complete state of this guard with another.
     * @details Exchanges ALL member variables including original_ and active_.
     */
    void swap(ValueGuard& other) noexcept(
        std::is_nothrow_swappable_v<T> &&
        std::is_nothrow_swappable_v<Policy>) 
    {
        using std::swap;
        swap(static_cast<Policy&>(*this), static_cast<Policy&>(other));
        swap(target_, other.target_);
        swap(original_, other.original_);
        swap(active_, other.active_);
    }
    
    friend void swap(ValueGuard& lhs, ValueGuard& rhs) noexcept(
        std::is_nothrow_swappable_v<T> &&
        std::is_nothrow_swappable_v<Policy>) 
    {
        lhs.swap(rhs);
    }

private:
    T* target_;
    OriginalType original_;
    bool active_;
};

// --- Deduction Guides (Constrained to remove ambiguity) ---

/**
 * @brief Deduction guide for copy construction.
 */
template <typename T>
ValueGuard(T&, const T&) -> ValueGuard<T, ValueGuardCopyPolicy<T>>;

/**
 * @brief Deduction guide for move construction.
 */
template <typename T>
ValueGuard(T&, T&&) -> ValueGuard<T, ValueGuardMovePolicy<T>>;

/**
 * @brief Deduction guide for custom restorer (copy new_value).
 * @note Constrained: F must be invocable with (T&, T&&) and NOT invocable with zero args.
 */
template <typename T, typename F,
          typename = std::enable_if_t<
              std::is_invocable_v<F, T&, T&&> && 
              !std::is_invocable_v<F>>>
ValueGuard(T&, const T&, F&&) -> ValueGuard<T, ValueGuardCustomPolicy<T, std::decay_t<F>>>;

/**
 * @brief Deduction guide for custom restorer (move new_value).
 * @note Constrained: F must be invocable with (T&, T&&) and NOT invocable with zero args.
 */
template <typename T, typename F,
          typename = std::enable_if_t<
              std::is_invocable_v<F, T&, T&&> && 
              !std::is_invocable_v<F>>>
ValueGuard(T&, T&&, F&&) -> ValueGuard<T, ValueGuardCustomPolicy<T, std::decay_t<F>>>;

/**
 * @brief Deduction guide for conditional policy.
 * @note Constrained: Cond must be invocable with zero args returning bool,
 * and NOT invocable with (T&, T&&).
 */
template <typename T, typename Cond,
          typename = std::enable_if_t<
              std::is_invocable_r_v<bool, Cond> && 
              !std::is_invocable_v<Cond, T&, T&&>>>
ValueGuard(T&, T&&, Cond&&) -> ValueGuard<T, ValueGuardConditionalPolicy<T, std::decay_t<Cond>>>;

// --- Factory Functions ---

/**
 * @brief Creates a ValueGuard with copy policy.
 */
template <typename T>
[[nodiscard]] auto make_value_guard(T& target, const T& new_value) {
    return ValueGuard<T, ValueGuardCopyPolicy<T>>(target, new_value);
}

/**
 * @brief Creates a ValueGuard with move policy.
 */
template <typename T>
[[nodiscard]] auto make_value_guard_move(T& target, T&& new_value) {
    return ValueGuard<T, ValueGuardMovePolicy<T>>(target, std::move(new_value));
}

/**
 * @brief Creates a ValueGuard with custom policy.
 */
template <typename T, typename F>
[[nodiscard]] auto make_value_guard_custom(T& target, const T& new_value, F&& restorer) {
    return ValueGuard<T, ValueGuardCustomPolicy<T, std::decay_t<F>>>(
        target, new_value, std::forward<F>(restorer));
}

/**
 * @brief Creates a ValueGuard with conditional policy.
 */
template <typename T, typename Cond>
[[nodiscard]] auto make_value_guard_conditional(T& target, T&& new_value, Cond&& condition) {
    return ValueGuard<T, ValueGuardConditionalPolicy<T, std::decay_t<Cond>>>(
        target, std::move(new_value), std::forward<Cond>(condition));
}

/**
 * @brief Creates a ValueGuard with no-restore policy.
 */
template <typename T>
[[nodiscard]] auto make_value_guard_no_restore(T& target, const T& new_value) {
    return ValueGuard<T, ValueGuardNoRestorePolicy<T>>(target, new_value);
}

// Specialization for is_value_guard type trait (primary template in FatPTypeTraits.h)
template <typename T, typename Policy>
struct is_value_guard<ValueGuard<T, Policy>> : std::true_type {};

} // namespace fat_p
