/**
 * @file StrongId.h
 * @brief Provides the StrongId template for creating strong, type-safe ID
 * wrappers with zero runtime overhead.
 *
 * @details The StrongId template wraps an underlying integral type (T) and
 * uses a unique Tag struct to create a distinct, compile-time type.
 *
 * @version 1
 *
 * Perf: Identical to raw integer (optimizes away completely).
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
#include <atomic> // For AtomicStrongId alias
#if FATP_HAS_CPP20
#include <compare> // For std::strong_ordering
#endif

#include "CheckedArithmetic.h" // For safe arithmetic in ops
#include "Expected.h" // For safe creation
#include "FatPTypeTraits.h" // For is_strong_id base template

namespace fat_p {

    // --- Policy for Custom Checks (Extensible) ---
    /**
     * @brief Default check policy: no additional validation.
     */
    struct NoCheckPolicy {
        template <typename T>
        static constexpr void check(T) noexcept {}
    };

    /**
     * @brief Example positive check policy.
     */
    struct PositiveCheckPolicy {
        template <typename T>
        static constexpr void check(T value) {
            if constexpr (std::is_signed_v<T>) {
                if (value < 0) {
                    throw std::invalid_argument("Negative ID value not allowed");
                }
            }
        }
    };

    // --- OpPolicy for Custom Arithmetic (Extensible) ---
    /**
     * @brief Default op policy: standard arithmetic with checks.
     */
    template <typename U>
    struct DefaultOpPolicy {
        static constexpr U add(U lhs, U rhs) { return checked_add(lhs, rhs); }
        static constexpr U sub(U lhs, U rhs) { return checked_sub(lhs, rhs); }
        static constexpr U mul(U lhs, U rhs) { return checked_mul(lhs, rhs); }
        static constexpr U div(U lhs, U rhs) { return checked_div(lhs, rhs); }
        static constexpr U mod(U lhs, U rhs) { return checked_mod(lhs, rhs); }
        static constexpr U neg(U val) {
            if constexpr (std::is_signed_v<U>) {
                if (val == std::numeric_limits<U>::min()) {
                    throw std::overflow_error("Negation overflow");
                }
            }
            return static_cast<U>(-val);
        }
        static constexpr U bit_and(U lhs, U rhs) { return checked_and(lhs, rhs); }
        static constexpr U bit_or(U lhs, U rhs) { return checked_or(lhs, rhs); }
        static constexpr U bit_xor(U lhs, U rhs) { return checked_xor(lhs, rhs); }
        static constexpr U bit_not(U val) { return static_cast<U>(~val); }
        static constexpr U left_shift(U lhs, U rhs) { return checked_left_shift(lhs, rhs); }
        static constexpr U right_shift(U lhs, U rhs) { return checked_right_shift(lhs, rhs); }
    };

    /**
     * @brief Unchecked op policy: raw arithmetic without overflow checks.
     * 
     * Use when maximum performance is required and inputs are known to be safe.
     * Behavior on overflow is undefined (same as raw integer arithmetic).
     */
    template <typename U>
    struct UncheckedOpPolicy {
        static constexpr U add(U lhs, U rhs) noexcept { return static_cast<U>(lhs + rhs); }
        static constexpr U sub(U lhs, U rhs) noexcept { return static_cast<U>(lhs - rhs); }
        static constexpr U mul(U lhs, U rhs) noexcept { return static_cast<U>(lhs * rhs); }
        static constexpr U div(U lhs, U rhs) noexcept { return static_cast<U>(lhs / rhs); }
        static constexpr U mod(U lhs, U rhs) noexcept { return static_cast<U>(lhs % rhs); }
        static constexpr U neg(U val) noexcept { return static_cast<U>(-val); }
        static constexpr U bit_and(U lhs, U rhs) noexcept { return static_cast<U>(lhs & rhs); }
        static constexpr U bit_or(U lhs, U rhs) noexcept { return static_cast<U>(lhs | rhs); }
        static constexpr U bit_xor(U lhs, U rhs) noexcept { return static_cast<U>(lhs ^ rhs); }
        static constexpr U bit_not(U val) noexcept { return static_cast<U>(~val); }
        static constexpr U left_shift(U lhs, U rhs) noexcept { return static_cast<U>(lhs << rhs); }
        static constexpr U right_shift(U lhs, U rhs) noexcept { return static_cast<U>(lhs >> rhs); }
    };

    // --- StrongId Class ---
    /**
     * @brief Strongly typed ID wrapper.
     *
     * @tparam T Underlying integral type.
     * @tparam Tag Unique type tag.
     * @tparam CheckPolicy Validation policy.
     * @tparam OpPolicy Arithmetic policy.
     */
    template <typename T, typename Tag, typename CheckPolicy = NoCheckPolicy,
        template <typename> class OpPolicy = DefaultOpPolicy>
    class StrongId {
        static_assert(std::is_integral_v<T>, "StrongId can only wrap integral types.");

    public:
        using value_type = T;

        /** @brief Default constructor. Initializes to 0 and validates. */
        constexpr StrongId() : m_value{} {
            CheckPolicy::check(m_value);
        }

        /** @brief Explicit constructor from value. Validates. */
        explicit constexpr StrongId(T value) : m_value(value) {
            CheckPolicy::check(m_value);
        }

        // Default copy/move/destruct (Optimal)
        constexpr StrongId(const StrongId&) = default;
        constexpr StrongId(StrongId&&) = default;
        constexpr StrongId& operator=(const StrongId&) = default;
        constexpr StrongId& operator=(StrongId&&) = default;

        /**
         * @brief Safe factory method.
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
        
        /** @brief Get underlying value. */
        [[nodiscard]] constexpr T get() const noexcept { return m_value; }
        
        /** @brief Get underlying value (alias). */
        [[nodiscard]] constexpr T value() const noexcept { return m_value; }

        /** @brief Explicit conversion to underlying type. */
        [[nodiscard]] explicit constexpr operator T() const noexcept { return m_value; }

        // --- Comparison ---
        [[nodiscard]] friend constexpr bool operator==(const StrongId& lhs, const StrongId& rhs) noexcept {
            return lhs.m_value == rhs.m_value;
        }
        [[nodiscard]] friend constexpr bool operator!=(const StrongId& lhs, const StrongId& rhs) noexcept {
            return lhs.m_value != rhs.m_value;
        }
        [[nodiscard]] friend constexpr bool operator<(const StrongId& lhs, const StrongId& rhs) noexcept {
            return lhs.m_value < rhs.m_value;
        }
        [[nodiscard]] friend constexpr bool operator<=(const StrongId& lhs, const StrongId& rhs) noexcept {
            return lhs.m_value <= rhs.m_value;
        }
        [[nodiscard]] friend constexpr bool operator>(const StrongId& lhs, const StrongId& rhs) noexcept {
            return lhs.m_value > rhs.m_value;
        }
        [[nodiscard]] friend constexpr bool operator>=(const StrongId& lhs, const StrongId& rhs) noexcept {
            return lhs.m_value >= rhs.m_value;
        }

#if FATP_HAS_CPP20
        [[nodiscard]] friend constexpr auto operator<=>(const StrongId& lhs, const StrongId& rhs) noexcept {
            return lhs.m_value <=> rhs.m_value;
        }
#endif

        // --- Arithmetic Ops ---
        constexpr StrongId& operator++() {
            m_value = OpPolicy<T>::add(m_value, T(1));
            CheckPolicy::check(m_value);
            return *this;
        }

        constexpr StrongId operator++(int) {
            StrongId temp = *this;
            ++(*this);
            return temp;
        }

        constexpr StrongId& operator--() {
            m_value = OpPolicy<T>::sub(m_value, T(1));
            CheckPolicy::check(m_value);
            return *this;
        }

        constexpr StrongId operator--(int) {
            StrongId temp = *this;
            --(*this);
            return temp;
        }

        constexpr StrongId& operator+=(const StrongId& rhs) {
            m_value = OpPolicy<T>::add(m_value, rhs.m_value);
            CheckPolicy::check(m_value);
            return *this;
        }
        constexpr StrongId& operator+=(T rhs) {
            m_value = OpPolicy<T>::add(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }

        constexpr StrongId& operator-=(const StrongId& rhs) {
            m_value = OpPolicy<T>::sub(m_value, rhs.m_value);
            CheckPolicy::check(m_value);
            return *this;
        }
        constexpr StrongId& operator-=(T rhs) {
            m_value = OpPolicy<T>::sub(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }

        constexpr StrongId& operator*=(const StrongId& rhs) {
            m_value = OpPolicy<T>::mul(m_value, rhs.m_value);
            CheckPolicy::check(m_value);
            return *this;
        }
        constexpr StrongId& operator*=(T rhs) {
            m_value = OpPolicy<T>::mul(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }

        constexpr StrongId& operator/=(const StrongId& rhs) {
            m_value = OpPolicy<T>::div(m_value, rhs.m_value);
            CheckPolicy::check(m_value);
            return *this;
        }
        constexpr StrongId& operator/=(T rhs) {
            m_value = OpPolicy<T>::div(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }

        constexpr StrongId& operator%=(const StrongId& rhs) {
            m_value = OpPolicy<T>::mod(m_value, rhs.m_value);
            CheckPolicy::check(m_value);
            return *this;
        }
        constexpr StrongId& operator%=(T rhs) {
            m_value = OpPolicy<T>::mod(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }

        // Binary Ops
        [[nodiscard]] constexpr friend StrongId operator+(StrongId lhs, const StrongId& rhs) { return lhs += rhs; }
        [[nodiscard]] constexpr friend StrongId operator+(StrongId lhs, T rhs) { return lhs += rhs; }
        [[nodiscard]] constexpr friend StrongId operator-(StrongId lhs, const StrongId& rhs) { return lhs -= rhs; }
        [[nodiscard]] constexpr friend StrongId operator-(StrongId lhs, T rhs) { return lhs -= rhs; }
        [[nodiscard]] constexpr friend StrongId operator*(StrongId lhs, const StrongId& rhs) { return lhs *= rhs; }
        [[nodiscard]] constexpr friend StrongId operator*(StrongId lhs, T rhs) { return lhs *= rhs; }
        [[nodiscard]] constexpr friend StrongId operator/(StrongId lhs, const StrongId& rhs) { return lhs /= rhs; }
        [[nodiscard]] constexpr friend StrongId operator/(StrongId lhs, T rhs) { return lhs /= rhs; }
        [[nodiscard]] constexpr friend StrongId operator%(StrongId lhs, const StrongId& rhs) { return lhs %= rhs; }
        [[nodiscard]] constexpr friend StrongId operator%(StrongId lhs, T rhs) { return lhs %= rhs; }

        // Unary Ops
        [[nodiscard]] constexpr StrongId operator-() const {
            return StrongId(OpPolicy<T>::neg(m_value));
        }
        [[nodiscard]] constexpr StrongId operator+() const {
            return *this;
        }

        // Bitwise Ops
        constexpr StrongId& operator&=(T rhs) {
            m_value = OpPolicy<T>::bit_and(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        [[nodiscard]] constexpr friend StrongId operator&(StrongId lhs, T rhs) { return lhs &= rhs; }

        constexpr StrongId& operator|=(T rhs) {
            m_value = OpPolicy<T>::bit_or(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        [[nodiscard]] constexpr friend StrongId operator|(StrongId lhs, T rhs) { return lhs |= rhs; }

        constexpr StrongId& operator^=(T rhs) {
            m_value = OpPolicy<T>::bit_xor(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        [[nodiscard]] constexpr friend StrongId operator^(StrongId lhs, T rhs) { return lhs ^= rhs; }

        [[nodiscard]] constexpr StrongId operator~() const {
            return StrongId(OpPolicy<T>::bit_not(m_value));
        }

        constexpr StrongId& operator<<=(T rhs) {
            m_value = OpPolicy<T>::left_shift(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        [[nodiscard]] constexpr friend StrongId operator<<(StrongId lhs, T rhs) { return lhs <<= rhs; }

        constexpr StrongId& operator>>=(T rhs) {
            m_value = OpPolicy<T>::right_shift(m_value, rhs);
            CheckPolicy::check(m_value);
            return *this;
        }
        [[nodiscard]] constexpr friend StrongId operator>>(StrongId lhs, T rhs) { return lhs >>= rhs; }

        // Swap
        constexpr void swap(StrongId& other) noexcept {
            using std::swap;
            swap(m_value, other.m_value);
        }

        friend constexpr void swap(StrongId& lhs, StrongId& rhs) noexcept {
            lhs.swap(rhs);
        }

    private:
        T m_value{};
    };

#if FATP_ENABLE_IOSTREAM
    template <typename T, typename Tag, typename Check, template <typename> class Op>
    std::ostream& operator<<(std::ostream& os, const StrongId<T, Tag, Check, Op>& id) {
        return os << id.get();
    }
#endif

    // --- Atomic StrongId Alias ---
    /**
     * @brief Atomic version of StrongId.
     * @details Uses std::atomic directly since StrongId is a trivially copyable wrapper.
     * This provides true lock-free thread safety (on supported hardware) without
     * internal mutex overhead.
     */
    template <typename T, typename Tag, typename CheckPolicy = NoCheckPolicy,
        template <typename> class OpPolicy = DefaultOpPolicy>
    using AtomicStrongId = std::atomic<StrongId<T, Tag, CheckPolicy, OpPolicy>>;

    // --- Type Trait Specialization ---
    // Base template is_strong_id defined in FatPTypeTraits.h
    template <typename T, typename Tag, typename V, template <typename> class O>
    struct is_strong_id<StrongId<T, Tag, V, O>> : std::true_type {};

} // namespace fat_p

// --- Hash Specialization ---
namespace std {
    template <typename T, typename Tag, typename Check, template <typename> class Op>
    struct hash<fat_p::StrongId<T, Tag, Check, Op>> {
        size_t operator()(const fat_p::StrongId<T, Tag, Check, Op>& id) const noexcept {
            return std::hash<T>{}(id.get());
        }
    };
}
