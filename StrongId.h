/**
 * @file StrongId.h
 * @brief Provides the StrongId template for creating strong, type-safe ID
 * wrappers with zero runtime overhead.
 *
 * @details The StrongId template wraps an underlying integral type (T) and
 * uses a unique Tag struct to create a distinct, compile-time type. This
 * prevents accidental mixing of different ID types (e.g., UserId and
 * TransactionId) while maintaining the memory footprint of the underlying
 * type (T). Includes DbC enforces in ctor, arithmetic ops with CheckedArithmetic,
 * constexpr where possible, and policy for custom checks.
 * 
 * @version 2.0 - THREAD-SAFE EDITION:
 *   - Added thread-safe assignment operators (copy/move)
 *   - Added read locking for const operations under SharedMutexPolicy
 *   - Added CheckPolicy validation in default constructor
 *   - Implemented modular division in ModularOpPolicy (Fermat's Little Theorem)
 *   - Added efficient swap() with proper locking
 *   - Improved constexpr support for lock-free operations
 *   - Enhanced AtomicStrongId documentation
 * 
 * Wish-List: Atomic wrap for thread-safe IDs; Expected for safe creation.
 * Perf: Matches plain T; constexpr ops.
 * Extensible: CheckPolicy for custom validation.
 */
#pragma once
#if !defined(FATP_ENABLE_IOSTREAM)
#define FATP_ENABLE_IOSTREAM 1 // Enable by default; undef to disable <iostream> for <<
#endif

#include "CppStandardDetection.h"

#if FATP_ENABLE_IOSTREAM
#include <iostream>
#endif
#include <utility>
#include <type_traits>
#include <functional>
#include <limits>
#include <stdexcept>
#if FATP_HAS_CPP20
#include <compare> // For std::strong_ordering in <=> operator
#endif

#include "CheckedArithmetic.h" // For safe arithmetic in ops
#include "Expected.h" // For safe creation (e.g., create)
#include "AtomicReference.h" // For atomic wrap (conditional)
#include "ConcurrencyPolicies.h" // For thread-safety in ops

namespace fat_p {
    // --- Policy for Custom Checks (Extensible) ---
    /**
     * @brief Default check policy: no additional validation.
     */
    struct NoCheckPolicy {
        template <typename T>
        static void check(T) noexcept {}
    };
    /**
     * @brief Example positive check policy.
     */
    struct PositiveCheckPolicy {
        template <typename T>
        static void check(T value) {
            if (value < 0) {
                throw std::invalid_argument("Negative ID value not allowed");
            }
        }
    };
    // --- OpPolicy for Custom Arithmetic (Extensible, e.g., modular) ---
    /**
     * @brief Default op policy: standard arithmetic with checks.
     */
    template <typename U>
    struct DefaultOpPolicy {
        static U add(U lhs, U rhs) { return checked_add(lhs, rhs); }
        static U sub(U lhs, U rhs) { return checked_sub(lhs, rhs); }
        static U mul(U lhs, U rhs) { return checked_mul(lhs, rhs); }
        static U div(U lhs, U rhs) { return checked_div(lhs, rhs); }
        static U mod(U lhs, U rhs) { return checked_mod(lhs, rhs); }
        static U neg(U val) { 
            // Negation: check for overflow (e.g., -INT_MIN)
            if constexpr (std::is_signed_v<U>) {
                if (val == std::numeric_limits<U>::min()) {
                    throw std::overflow_error("Negation overflow");
                }
            }
            return static_cast<U>(-val);
        }
        static U bit_and(U lhs, U rhs) { return checked_and(lhs, rhs); }
        static U bit_or(U lhs, U rhs) { return checked_or(lhs, rhs); }
        static U bit_xor(U lhs, U rhs) { return checked_xor(lhs, rhs); }
        static U bit_not(U val) { return static_cast<U>(~val); }
        static U left_shift(U lhs, U rhs) { return checked_left_shift(lhs, rhs); }
        static U right_shift(U lhs, U rhs) { return checked_right_shift(lhs, rhs); }
    };
    
    /**
     * @brief Helper for compile-time primality testing (used by ModularOpPolicy).
     */
    namespace detail {
        constexpr bool is_prime(unsigned long long n) {
            if (n < 2) return false;
            if (n == 2) return true;
            if (n % 2 == 0) return false;
            for (unsigned long long i = 3; i * i <= n; i += 2) {
                if (n % i == 0) return false;
            }
            return true;
        }
    }
    
    /**
     * @brief Modular arithmetic implementation (internal).
     * @tparam T Type of the value
     * @tparam M Modulus (must be prime for division support).
     */
    template <typename T, T M>
    struct ModularOpPolicyImpl {
        static_assert(std::is_integral_v<T>, "T must be an integral type");
        static_assert(M > 0, "Modulus M must be positive");
        static_assert(detail::is_prime(static_cast<unsigned long long>(M)), 
                      "Modulus M must be prime for modular division (requires inverse)");

        static constexpr T add(T lhs, T rhs) {
            using Wide = std::conditional_t<std::is_signed_v<T>, long long, unsigned long long>;
            return static_cast<T>((static_cast<Wide>(lhs) + rhs) % M);
        }

        static constexpr T sub(T lhs, T rhs) {
            using Wide = std::conditional_t<std::is_signed_v<T>, long long, unsigned long long>;
            return static_cast<T>(((static_cast<Wide>(lhs) - rhs) % M + M) % M);  // Ensure non-negative
        }

        static constexpr T mul(T lhs, T rhs) {
            using Wide = std::conditional_t<std::is_signed_v<T>, long long, unsigned long long>;
            return static_cast<T>((static_cast<Wide>(lhs) * rhs) % M);
        }
        
        /**
         * @brief Compute modular inverse using Fermat's Little Theorem: a^(M-2) mod M
         * @details Only valid when M is prime. For non-prime M, use extended Euclidean algorithm.
         */
        static constexpr T mod_inverse(T a) {
            T res = 1;
            T base = a % M;
            T exp = M - 2;
            
            // Fast modular exponentiation
            while (exp > 0) {
                if (exp % 2 == 1) {
                    res = mul(res, base);
                }
                base = mul(base, base);
                exp /= 2;
            }
            return res;
        }

        /**
         * @brief Modular division: lhs / rhs mod M = lhs * inverse(rhs) mod M
         */
        static constexpr T div(T lhs, T rhs) {
            if (rhs == 0) {
                throw std::invalid_argument("Division by zero");
            }
            return mul(lhs, mod_inverse(rhs));
        }

        static constexpr T mod(T lhs, T rhs) {
            using Wide = std::conditional_t<std::is_signed_v<T>, long long, unsigned long long>;
            return static_cast<T>((static_cast<Wide>(lhs) % rhs + rhs) % rhs); // Handle negative
        }

        static constexpr T neg(T val) {
            return (M - (val % M)) % M; // Modular negation
        }

        static constexpr T bit_and(T lhs, T rhs) {
            return (lhs & rhs) % M; // Bitwise with mod wrap
        }

        static constexpr T bit_or(T lhs, T rhs) {
            return (lhs | rhs) % M;
        }

        static constexpr T bit_xor(T lhs, T rhs) {
            return (lhs ^ rhs) % M;
        }

        static constexpr T bit_not(T val) {
            return (~val) % M;
        }

        static constexpr T left_shift(T lhs, T rhs) {
            return (lhs << rhs) % M;
        }

        static constexpr T right_shift(T lhs, T rhs) {
            return (lhs >> rhs) % M;
        }
    };
    
    /**
     * @brief Modular arithmetic policy wrapper (mod M) for use with StrongId.
     * @details Use ModularOpPolicy<M>::template Policy<T> or create alias:
     * template<typename T> using MyModPolicy = ModularOpPolicy<7>::Policy<T>;
     * @tparam M Modulus (must be prime for division support).
     */
    template <auto M>
    struct ModularOpPolicy {
        template <typename T>
        using Policy = ModularOpPolicyImpl<T, static_cast<T>(M)>;
    };

    // --- StrongId Class ---
    /**
     * @brief Base template for a strongly typed ID wrapper.
     *
     * @tparam T The underlying integral type (e.g., int, long) that holds the
     * raw identifier value.
     * @tparam Tag A unique, empty struct used solely to differentiate types
     * at compile time.
     * @tparam CheckPolicy Policy for custom validation in ctor (defaults NoCheckPolicy).
     * @tparam ConcurrencyPolicy Policy for thread-safety in ops (defaults SingleThreadedPolicy).
     * @tparam OpPolicy Policy for custom arithmetic ops (defaults DefaultOpPolicy).
     */
    template <typename T, typename Tag, typename CheckPolicy = NoCheckPolicy,
        typename ConcurrencyPolicy = SingleThreadedPolicy,
        template <typename> class OpPolicy = DefaultOpPolicy>
    class StrongId : public ConcurrencyPolicy {
        // Contract: Enforce that T is an integral type before use.
        static_assert(std::is_integral_v<T>,
            "StrongId can only wrap integral types.");
    public:
        /**
         * @brief Type alias for the underlying identifier storage type.
         */
        using value_type = T;
        
        /**
         * @brief Default constructor with CheckPolicy validation.
         * @details Initializes the underlying value to T{} and validates with CheckPolicy.
         */
        constexpr StrongId() : m_value{} {
            CheckPolicy::check(m_value);
        }
        
        /**
         * @brief Explicit constructor from the underlying value type.
         * @details The 'explicit' keyword prevents implicit conversion from T
         * to StrongId, enforcing strong type safety. Applies CheckPolicy.
         * @param value The initial raw value of the ID.
         */
        explicit constexpr StrongId(T value) : m_value(value) {
            CheckPolicy::check(m_value);
        }
        
        /**
         * @brief Copy constructor.
         */
        StrongId(const StrongId& other) : ConcurrencyPolicy(), m_value(other.m_value) {
            CheckPolicy::check(m_value);
        }
        
        /**
         * @brief Move constructor.
         */
        StrongId(StrongId&& other) noexcept(std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) 
            : ConcurrencyPolicy(), m_value(std::move(other.m_value)) {
            CheckPolicy::check(m_value);
        }
        
        /**
         * @brief Copy assignment operator with thread-safe locking.
         * @details Acquires exclusive lock on this, shared lock on other for safe copying.
         */
        StrongId& operator=(const StrongId& other) {
            if (this != &other) {
                if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                    typename ConcurrencyPolicy::LockGuard guard_this(this->getLock());
                    typename ConcurrencyPolicy::SharedGuard guard_other(other.getLock());
                    m_value = other.m_value;
                } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                    typename ConcurrencyPolicy::LockGuard guard_this(this->getLock());
                    typename ConcurrencyPolicy::LockGuard guard_other(other.getLock());
                    m_value = other.m_value;
                } else {
                    m_value = other.m_value;
                }
                CheckPolicy::check(m_value);
            }
            return *this;
        }
        
        /**
         * @brief Move assignment operator with thread-safe locking.
         * @details Acquires exclusive locks on both this and other for safe move.
         */
        StrongId& operator=(StrongId&& other) noexcept(std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
            if (this != &other) {
                if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                    typename ConcurrencyPolicy::LockGuard guard_this(this->getLock());
                    typename ConcurrencyPolicy::LockGuard guard_other(other.getLock());
                    m_value = std::move(other.m_value);
                } else {
                    m_value = std::move(other.m_value);
                }
                CheckPolicy::check(m_value);
            }
            return *this;
        }
        
        // --- Safe Creation with Expected (Wish-List) ---
        /**
         * @brief Creates a StrongId safely, returning Expected on check failure.
         * @param value The raw value.
         * @return Expected<StrongId, std::string>
         */
        static Expected<StrongId, std::string> create(T value) {
            try {
                CheckPolicy::check(value);
                return StrongId(value);
            }
            catch (const std::exception& e) {
                return make_unexpected(e.what());
            }
        }
        
        // --- Accessors ---
        /**
         * @brief Explicitly retrieves the underlying raw value with thread-safe read.
         * @return The raw ID value.
         */
        [[nodiscard]] T get() const noexcept {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard(this->getLock());
                return m_value;
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard(this->getLock());
                return m_value;
            } else {
                return m_value;
            }
        }
        
        /**
         * @brief Implicit conversion to the underlying type (for convenience).
         * @return The raw ID value.
         */
        [[nodiscard]] explicit operator T() const noexcept {
            return get();
        }
        
        // --- Comparison Ops ---
        [[nodiscard]] friend bool operator==(const StrongId& lhs, const StrongId& rhs) noexcept {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard_lhs(lhs.getLock());
                typename ConcurrencyPolicy::SharedGuard guard_rhs(rhs.getLock());
                return lhs.m_value == rhs.m_value;
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard_lhs(lhs.getLock());
                typename ConcurrencyPolicy::LockGuard guard_rhs(rhs.getLock());
                return lhs.m_value == rhs.m_value;
            } else {
                return lhs.m_value == rhs.m_value;
            }
        }
        
        [[nodiscard]] friend bool operator!=(const StrongId& lhs, const StrongId& rhs) noexcept {
            return !(lhs == rhs);
        }
        
        [[nodiscard]] friend bool operator<(const StrongId& lhs, const StrongId& rhs) noexcept {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard_lhs(lhs.getLock());
                typename ConcurrencyPolicy::SharedGuard guard_rhs(rhs.getLock());
                return lhs.m_value < rhs.m_value;
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard_lhs(lhs.getLock());
                typename ConcurrencyPolicy::LockGuard guard_rhs(rhs.getLock());
                return lhs.m_value < rhs.m_value;
            } else {
                return lhs.m_value < rhs.m_value;
            }
        }
        
        [[nodiscard]] friend bool operator<=(const StrongId& lhs, const StrongId& rhs) noexcept {
            return !(rhs < lhs);
        }
        
        [[nodiscard]] friend bool operator>(const StrongId& lhs, const StrongId& rhs) noexcept {
            return rhs < lhs;
        }
        
        [[nodiscard]] friend bool operator>=(const StrongId& lhs, const StrongId& rhs) noexcept {
            return !(lhs < rhs);
        }
#if FATP_HAS_CPP20
        [[nodiscard]] friend std::strong_ordering operator<=>(const StrongId& lhs, const StrongId& rhs) noexcept {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard_lhs(lhs.getLock());
                typename ConcurrencyPolicy::SharedGuard guard_rhs(rhs.getLock());
                return lhs.m_value <=> rhs.m_value;
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard_lhs(lhs.getLock());
                typename ConcurrencyPolicy::LockGuard guard_rhs(rhs.getLock());
                return lhs.m_value <=> rhs.m_value;
            } else {
                return lhs.m_value <=> rhs.m_value;
            }
        }
#endif
        // --- Arithmetic Ops ---
        StrongId& operator++() {
            if constexpr (std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy> &&
                          noexcept(CheckPolicy::check(std::declval<T>()))) {
                CheckPolicy::check(m_value);
                m_value = OpPolicy<T>::add(m_value, T(1));
                CheckPolicy::check(m_value);
                return *this;
            } else {
                typename ConcurrencyPolicy::LockGuard guard(this->getLock());
                CheckPolicy::check(m_value);
                m_value = OpPolicy<T>::add(m_value, T(1));
                CheckPolicy::check(m_value);
                return *this;
            }
        }
        
        StrongId operator++(int) {
            StrongId temp = *this;
            ++(*this);
            return temp;
        }
        
        StrongId& operator--() {
            if constexpr (std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy> &&
                          noexcept(CheckPolicy::check(std::declval<T>()))) {
                CheckPolicy::check(m_value);
                m_value = OpPolicy<T>::sub(m_value, T(1));
                CheckPolicy::check(m_value);
                return *this;
            } else {
                typename ConcurrencyPolicy::LockGuard guard(this->getLock());
                CheckPolicy::check(m_value);
                m_value = OpPolicy<T>::sub(m_value, T(1));
                CheckPolicy::check(m_value);
                return *this;
            }
        }
        
        StrongId operator--(int) {
            StrongId temp = *this;
            --(*this);
            return temp;
        }
        
        StrongId& operator+=(T rhs) {
            typename ConcurrencyPolicy::LockGuard guard(this->getLock());
            CheckPolicy::check(m_value);
            m_value = OpPolicy<T>::add(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        
        StrongId operator+(T rhs) const {
            StrongId temp = *this;
            temp += rhs;
            return temp;
        }
        
        StrongId operator+(const StrongId& rhs) const {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::SharedGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::add(m_value, rhs.m_value));
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::LockGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::add(m_value, rhs.m_value));
            } else {
                return StrongId(OpPolicy<T>::add(m_value, rhs.m_value));
            }
        }
        
        StrongId& operator-=(T rhs) {
            typename ConcurrencyPolicy::LockGuard guard(this->getLock());
            CheckPolicy::check(m_value);
            m_value = OpPolicy<T>::sub(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        
        StrongId operator-(T rhs) const {
            StrongId temp = *this;
            temp -= rhs;
            return temp;
        }
        
        StrongId operator-(const StrongId& rhs) const {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::SharedGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::sub(m_value, rhs.m_value));
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::LockGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::sub(m_value, rhs.m_value));
            } else {
                return StrongId(OpPolicy<T>::sub(m_value, rhs.m_value));
            }
        }
        
        StrongId& operator*=(T rhs) {
            typename ConcurrencyPolicy::LockGuard guard(this->getLock());
            CheckPolicy::check(m_value);
            m_value = OpPolicy<T>::mul(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        
        StrongId operator*(T rhs) const {
            StrongId temp = *this;
            temp *= rhs;
            return temp;
        }
        
        StrongId operator*(const StrongId& rhs) const {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::SharedGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::mul(m_value, rhs.m_value));
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::LockGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::mul(m_value, rhs.m_value));
            } else {
                return StrongId(OpPolicy<T>::mul(m_value, rhs.m_value));
            }
        }
        
        StrongId& operator/=(T rhs) {
            typename ConcurrencyPolicy::LockGuard guard(this->getLock());
            CheckPolicy::check(m_value);
            m_value = OpPolicy<T>::div(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        
        StrongId operator/(T rhs) const {
            StrongId temp = *this;
            temp /= rhs;
            return temp;
        }
        
        StrongId operator/(const StrongId& rhs) const {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::SharedGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::div(m_value, rhs.m_value));
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::LockGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::div(m_value, rhs.m_value));
            } else {
                return StrongId(OpPolicy<T>::div(m_value, rhs.m_value));
            }
        }
        
        StrongId& operator%=(T rhs) {
            typename ConcurrencyPolicy::LockGuard guard(this->getLock());
            CheckPolicy::check(m_value);
            m_value = OpPolicy<T>::mod(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        
        StrongId operator%(T rhs) const {
            StrongId temp = *this;
            temp %= rhs;
            return temp;
        }
        
        StrongId operator%(const StrongId& rhs) const {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::SharedGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::mod(m_value, rhs.m_value));
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::LockGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::mod(m_value, rhs.m_value));
            } else {
                return StrongId(OpPolicy<T>::mod(m_value, rhs.m_value));
            }
        }
        
        // --- Unary Arithmetic Ops ---
        StrongId operator-() const {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard(this->getLock());
                return StrongId(OpPolicy<T>::neg(m_value));
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard(this->getLock());
                return StrongId(OpPolicy<T>::neg(m_value));
            } else {
                return StrongId(OpPolicy<T>::neg(m_value));
            }
        }
        
        StrongId operator+() const {
            return *this;
        }
        
        // --- Bitwise Ops ---
        StrongId& operator&=(T rhs) {
            typename ConcurrencyPolicy::LockGuard guard(this->getLock());
            CheckPolicy::check(m_value);
            m_value = OpPolicy<T>::bit_and(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        
        StrongId operator&(T rhs) const {
            StrongId temp = *this;
            temp &= rhs;
            return temp;
        }
        
        StrongId operator&(const StrongId& rhs) const {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::SharedGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::bit_and(m_value, rhs.m_value));
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::LockGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::bit_and(m_value, rhs.m_value));
            } else {
                return StrongId(OpPolicy<T>::bit_and(m_value, rhs.m_value));
            }
        }
        
        StrongId& operator|=(T rhs) {
            typename ConcurrencyPolicy::LockGuard guard(this->getLock());
            CheckPolicy::check(m_value);
            m_value = OpPolicy<T>::bit_or(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        
        StrongId operator|(T rhs) const {
            StrongId temp = *this;
            temp |= rhs;
            return temp;
        }
        
        StrongId operator|(const StrongId& rhs) const {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::SharedGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::bit_or(m_value, rhs.m_value));
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::LockGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::bit_or(m_value, rhs.m_value));
            } else {
                return StrongId(OpPolicy<T>::bit_or(m_value, rhs.m_value));
            }
        }
        
        StrongId& operator^=(T rhs) {
            typename ConcurrencyPolicy::LockGuard guard(this->getLock());
            CheckPolicy::check(m_value);
            m_value = OpPolicy<T>::bit_xor(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        
        StrongId operator^(T rhs) const {
            StrongId temp = *this;
            temp ^= rhs;
            return temp;
        }
        
        StrongId operator^(const StrongId& rhs) const {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::SharedGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::bit_xor(m_value, rhs.m_value));
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::LockGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::bit_xor(m_value, rhs.m_value));
            } else {
                return StrongId(OpPolicy<T>::bit_xor(m_value, rhs.m_value));
            }
        }
        
        StrongId operator~() const {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard(this->getLock());
                return StrongId(OpPolicy<T>::bit_not(m_value));
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard(this->getLock());
                return StrongId(OpPolicy<T>::bit_not(m_value));
            } else {
                return StrongId(OpPolicy<T>::bit_not(m_value));
            }
        }
        
        StrongId& operator<<=(T rhs) {
            typename ConcurrencyPolicy::LockGuard guard(this->getLock());
            CheckPolicy::check(m_value);
            m_value = OpPolicy<T>::left_shift(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        
        StrongId operator<<(T rhs) const {
            StrongId temp = *this;
            temp <<= rhs;
            return temp;
        }
        
        StrongId operator<<(const StrongId& rhs) const {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::SharedGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::left_shift(m_value, rhs.m_value));
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::LockGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::left_shift(m_value, rhs.m_value));
            } else {
                return StrongId(OpPolicy<T>::left_shift(m_value, rhs.m_value));
            }
        }
        
        StrongId& operator>>=(T rhs) {
            typename ConcurrencyPolicy::LockGuard guard(this->getLock());
            CheckPolicy::check(m_value);
            m_value = OpPolicy<T>::right_shift(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        
        StrongId operator>>(T rhs) const {
            StrongId temp = *this;
            temp >>= rhs;
            return temp;
        }
        
        StrongId operator>>(const StrongId& rhs) const {
            if constexpr (is_shared_policy_v<ConcurrencyPolicy>) {
                typename ConcurrencyPolicy::SharedGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::SharedGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::right_shift(m_value, rhs.m_value));
            } else if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard_lhs(this->getLock());
                typename ConcurrencyPolicy::LockGuard guard_rhs(rhs.getLock());
                return StrongId(OpPolicy<T>::right_shift(m_value, rhs.m_value));
            } else {
                return StrongId(OpPolicy<T>::right_shift(m_value, rhs.m_value));
            }
        }
        
        // --- Swap Function ---
        /**
         * @brief Efficient swap with thread-safe locking.
         */
        void swap(StrongId& other) noexcept {
            if constexpr (!std::is_same_v<ConcurrencyPolicy, SingleThreadedPolicy>) {
                typename ConcurrencyPolicy::LockGuard guard_this(this->getLock());
                typename ConcurrencyPolicy::LockGuard guard_other(other.getLock());
                std::swap(m_value, other.m_value);
            } else {
                std::swap(m_value, other.m_value);
            }
        }
        
        /**
         * @brief Friend swap for ADL (Argument-Dependent Lookup).
         */
        friend void swap(StrongId& lhs, StrongId& rhs) noexcept {
            lhs.swap(rhs);
        }
        
    private:
        /**
         * @brief The actual storage for the raw identifier value.
         * @details Initialized via non-static data member initialization.
         */
        T m_value{};
    };
    
#if FATP_ENABLE_IOSTREAM
    // --- External Utilities ---
    /**
     * @brief Overload for standard output streams (e.g., std::cout).
     * @details Allows StrongId objects to be directly printed for debugging
     * and logging purposes.
     * @tparam T The underlying integral type.
     * @tparam Tag The unique tag.
     * @param os The output stream reference.
     * @param id The StrongId object to stream.
     * @return Reference to the output stream, enabling chaining.
     */
    template <typename T, typename Tag, typename CheckPolicy, typename ConcurrencyPolicy, template <typename> class OpPolicy>
    ::std::ostream& operator<<(::std::ostream& os, const StrongId<T, Tag, CheckPolicy, ConcurrencyPolicy, OpPolicy>& id) {
        // Outputs the underlying value only, ensuring clarity.
        os << id.get();
        return os;
    }
#endif
    
    // --- Atomic Wrap for Thread-Safe IDs (Conditional Thread-Safety) ---
    /**
     * @brief Atomic version of StrongId for thread-safe usage.
     * @details Use with SingleThreadedPolicy for StrongId; AtomicReference provides atomicity.
     * Avoid combining MultiThreadedPolicy with AtomicStrongId to prevent duplicate locking.
     * 
     * @example
     * // Correct usage:
     * using AtomicUserId = AtomicStrongId<int, struct UserTag>;
     * AtomicUserId atomic_id(42);
     * 
     * // Access using AtomicReference API:
     * auto id = atomic_id.load();
     * atomic_id.store(StrongId<int, struct UserTag>(100));
     * 
     * @tparam T Underlying type.
     * @tparam Tag Unique tag.
     * @tparam CheckPolicy Check policy.
     * @tparam ConcurrencyPolicy Concurrency policy (should be SingleThreadedPolicy for atomic).
     * @tparam OpPolicy Op policy.
     */
    template <typename T, typename Tag, typename CheckPolicy = NoCheckPolicy,
        typename ConcurrencyPolicy = SingleThreadedPolicy,
        template <typename> class OpPolicy = DefaultOpPolicy>
    using AtomicStrongId = AtomicReference<StrongId<T, Tag, CheckPolicy, ConcurrencyPolicy, OpPolicy>>;
    
} // namespace fat_p

// --- Hash Specialization for Standard Containers ---
/**
 * @brief Specialization of std::hash for StrongId.
 * @details This specialization is required to allow StrongId objects to
 * function as keys in hash-based containers like std::unordered_map.
 */
namespace std {
    template <typename T, typename Tag, typename CheckPolicy, typename ConcurrencyPolicy, template <typename> class OpPolicy>
    struct hash<fat_p::StrongId<T, Tag, CheckPolicy, ConcurrencyPolicy, OpPolicy>> {
        /**
         * @brief Hash functor operator.
         * @param id The StrongId object to hash.
         * @return The hash value (size_t) derived from the underlying T.
         */
        [[nodiscard]] size_t operator()(
            const fat_p::StrongId<T, Tag, CheckPolicy, ConcurrencyPolicy, OpPolicy>& id) const noexcept
        {
            // Delegates hashing to the underlying type T's standard hash.
            return std::hash<T>{}(id.get());
        }
    };
} // namespace std

namespace fat_p {

template <typename T, typename Tag, typename V, typename C, template <typename> class O>
struct is_strong_id<StrongId<T, Tag, V, C, O>> : std::true_type {};

}
