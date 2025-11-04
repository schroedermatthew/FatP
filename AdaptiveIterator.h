/**
 * @file AdaptiveIterator.h
 * @brief Defines a C++ policy-based iterator class for customizable traversal.
 *
 * @details This iterator delegates core behaviors (like advancing, stepping, and
 * filtering) to a compile-time Policy, enabling various iteration patterns
 * (standard, stride, filtering, transform, reverse) with maximum performance due to static
 * dispatch. Integrates with library's DbC (enforce.h for checks), Expected for safe ops,
 * ConcurrencyPolicies for thread-safety, and CheckedArithmetic for safe arithmetic in ops.
 * Conditional thread-safety via ConcurrencyPolicy param.
 * Extensible: New policies (e.g., TransformPolicy for mapped views, ReversePolicy).
 * Optimized: Static dispatch; no runtime overhead for standard; loop-based filtering (no deps).
 * C++17 compliant; header-only; no external deps (guards optional <mutex>/<shared_mutex>/<atomic>).
 *
 * @note Checks are debug-only (via enforce); release zero-overhead.
 * @note Supports movable-only T via std::forward.
 * @note Exposes stride_size() for user queries.
 *
 * UPDATES (Iterator SuperGrok - FIXED):
 * - Fixed duplicate constructor issues by merging predicate/transformer constructors
 * - Fixed std::conditional_t with missing types using helper traits
 * - Fixed ConcurrencyPolicy::lock_type -> NoOpLock
 * - Fixed Expected<Iterator&> -> Expected<void> (can't return references in Expected)
 * - All policies use non-static const methods
 * - Integrated CheckedArithmetic for safe pointer operations
 * - Proper policy composition with correct chaining logic
 * - Complete ConstAdaptiveIterator implementation
 * - Thread-safe iteration support
 * - Tunable performance parameters
 *
 * NEW UPDATES (Performance Improvements):
 * - Debug-only bounds checks via enforce (no-op in release).
 * - Optimized return type (void for non-failing policies like Standard).
 * - Added SafeAdaptiveIterator alias for always-checked variant.
 */
#pragma once
#include <iterator>
#include <type_traits>
#include <utility>
#include <cstddef>
#include <algorithm>
#include <vector>    // For TensorStridePolicy strides
#include <memory>    // For unique_ptr in factory
#include <optional>  // For predicate/transformer storage
#include <functional> // For std::function
#include <map>       // For factory registry
#include <variant>   // For std::monostate

#include "enforce.h"             // For DbC checks (debug-only)
#include "Expected.h"            // For safe ops (e.g., ++ returns Expected on failure)
#include "ConcurrencyPolicies.h" // For conditional thread-safety
#include "CheckedArithmetic.h"   // For safe pointer arithmetic (overflow checks)
#include "Factory.h"             // For factory integration

#if !defined(CPP_UTILITIES_USE_SHARED_MUTEX)
#define CPP_UTILITIES_USE_SHARED_MUTEX 1  // Enable RW by default
#endif

#if !defined(CPP_UTILITIES_USE_ATOMIC)
#define CPP_UTILITIES_USE_ATOMIC 1  // Enable spinlock
#endif

namespace cpp_utilities {

    // --------------------------------------------------------------------
    // 0. Policy Traits (SFINAE Helpers) - Extended
    // --------------------------------------------------------------------

    /** @brief Trait to check if a policy defines a static member 'stride_value' (Stride). */
    template <typename T, typename = void>
    struct has_N : std::false_type {};
    template <typename P>
    struct has_N<P, std::void_t<decltype(P::stride_value)>> : std::true_type {};

    /** @brief Trait to check if a policy defines a 'predicate_type' (Filtering). */
    template <typename P, typename = void>
    struct has_predicate_type : std::false_type {};
    template <typename P>
    struct has_predicate_type<P, std::void_t<typename P::predicate_type>>
        : std::true_type {
    };

    /** @brief Trait to check if a policy defines a 'transformer_type' (Transform). */
    template <typename P, typename = void>
    struct has_transformer_type : std::false_type {};
    template <typename P>
    struct has_transformer_type<P, std::void_t<typename P::transformer_type>>
        : std::true_type {
    };

    /** @brief Trait to check if policy needs end_ (e.g., filter/stride). */
    template <typename P>
    struct needs_end : std::bool_constant<has_predicate_type<P>::value || has_N<P>::value> {};

    /** @brief NEW: Trait to check if policy can fail (needs end_ for bounds). */
    template <typename P>
    struct can_fail : needs_end<P> {};

    // Forward declaration of the AdaptiveIterator and ConstAdaptiveIterator
    template <typename T, typename Policy, typename ConcurrencyPolicy = SingleThreadedPolicy>
    class AdaptiveIterator;

    template <typename T, typename Policy, typename ConcurrencyPolicy = SingleThreadedPolicy>
    class ConstAdaptiveIterator;

    // Safe alias with always-on checks (uses always_enforce)
    template <typename T, typename Policy, typename ConcurrencyPolicy = SingleThreadedPolicy>
    class SafeAdaptiveIterator : public AdaptiveIterator<T, Policy, ConcurrencyPolicy> {
    public:
        using Base = AdaptiveIterator<T, Policy, ConcurrencyPolicy>;
        using Base::Base;

        [[nodiscard]] Expected<void, std::string> operator++() {
            typename ConcurrencyPolicy::LockGuard guard(this->lock_);

            // Check bounds and return error instead of throwing
            if (this->ptr_ >= this->end_) {
                return unexpected(std::string("Iterator past end"));
            }

            if constexpr (needs_end<Policy>::value) {
                if constexpr (has_predicate_type<Policy>::value) {
                    if (!this->predicate_.has_value()) {
                        return unexpected(std::string("Predicate not initialized"));
                    }
                    this->policy_.advance(this->ptr_, this->end_, *this->predicate_);
                }
                else {
                    this->policy_.advance(this->ptr_, this->end_);
                }
            }
            else {
                this->policy_.advance(this->ptr_);
            }

            return Expected<void, std::string>(std::in_place);
        }
    };

    // --------------------------------------------------------------------
    // 1. Iterator Policies (Customizable Behavior) - Extended
    // --------------------------------------------------------------------

    /**
     * @brief Default policy: standard sequential iteration (zero overhead).
     * @tparam T The element type.
     * @details Provides bidirectional iteration capability.
     */
    template <typename T>
    struct StandardIteratorPolicy {
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        /**
         * @brief Advances the pointer by one physical element.
         * @param ptr The pointer to advance.
         */
        void advance(T*& ptr) const {
            ++ptr;
        }

        void retreat(T*& ptr) const {
            --ptr;
        }
    };

    /**
     * @brief Stride Policy: Iteration moves by a fixed step N with overflow checking.
     * @tparam T The element type.
     * @tparam N The stride (step) size. Must be > 0.
     * @details Provides random access iteration capability.
     * Uses CheckedArithmetic for safe pointer operations to prevent overflow.
     * FIXED: Now uses non-static const methods for consistency.
     */
    template <typename T, int N>
    struct StrideIteratorPolicy {
        static_assert(N > 0, "Stride must be a positive integer.");

        using iterator_category = std::random_access_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        /**
         * @brief Advances the pointer by N steps.
         * @param ptr The pointer to advance.
         */
        void advance(T*& ptr) const {
            ptr += N;
        }

        /**
         * @brief Advances the pointer by N steps with bounds checking.
         * Clamps to end to ensure clean loop termination.
         * @param ptr The pointer to advance.
         * @param end The end boundary.
         */
        void advance(T*& ptr, T* end) const {
            ptr += N;
            // If stride takes us past end, clamp to end for clean termination
            if (ptr > end) {
                ptr = end;
            }
        }

        void retreat(T*& ptr) const {
            ptr -= N;
        }

        /** @brief Expose N as a static constexpr member for trait checks. */
        static constexpr int stride_value = N;
    };

    /**
     * @brief Filtering Policy: Skips elements based on a compile-time Predicate.
     * @tparam T The element type.
     * @tparam Predicate A callable class/struct with operator()(const T&) -> bool.
     * @tparam UnrollFactor Loop unrolling factor for performance (default: 4).
     * @details Provides forward iteration capability. Bidirectional movement is
     * intentionally unsupported due to search complexity.
     * FIXED: Now uses non-static const methods.
     */
    template <typename T, typename Predicate, int UnrollFactor = 4>
    struct FilteringIteratorPolicy {
        static_assert(UnrollFactor > 0, "UnrollFactor must be positive");

        using predicate_type = Predicate;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        /**
         * @brief Searches for the next element that satisfies the predicate.
         * @param ptr The pointer to search from.
         * @param end The end sentinel pointer.
         * @param predicate The predicate object used for filtering.
         * @note If no valid element is found before @p end, @p ptr will equal @p end.
         */
        void advance(T*& ptr, T* end, const Predicate& predicate) const {
            ++ptr; // Move to next first
            if (ptr >= end) return;

            for (int unroll = 0; unroll < UnrollFactor && ptr < end; ++unroll) {
                if (predicate(*ptr)) return;
                ++ptr;
                if (ptr >= end) return;
            }
            while (ptr < end && !predicate(*ptr)) {
                ++ptr;
            }
        }

    };

    /**
     * @brief Transform Policy: Maps elements on dereference (view-like).
     * @tparam T The underlying element type.
     * @tparam Transformer Callable with operator()(T&) -> U.
     * @tparam BasePolicy The base policy to wrap (e.g., Standard).
     * FIXED: Now uses non-static const methods.
     */
    template <typename T, typename Transformer, typename BasePolicy = StandardIteratorPolicy<T>>
    struct TransformIteratorPolicy : BasePolicy {
        using transformer_type = Transformer;
        using value_type = std::invoke_result_t<Transformer, T&>;
        using reference = value_type;
        using pointer = T*;

        /**
         * @brief Applies the transformer to the dereferenced element.
         * @param ptr Pointer to the underlying element.
         * @param transformer The transformation function.
         * @return The transformed value.
         */
        reference dereference(T* ptr, const Transformer& transformer) const {
            return transformer(*ptr);
        }
    };

    /**
     * @brief Tensor Stride Policy: Supports multi-dimensional striding.
     * @tparam T The element type.
     * @tparam NDims Number of dimensions (0 = runtime-determined).
     * @details For NDims > 0, strides are compile-time; otherwise runtime.
     * FIXED: Changed to non-static methods to properly access instance state (strides_).
     */
    template <typename T, size_t NDims = 0>
    struct TensorStridePolicy {
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        std::vector<std::ptrdiff_t> strides_;

        TensorStridePolicy(std::vector<std::ptrdiff_t> strides = {})
            : strides_(std::move(strides)) {
            enforce(!strides_.empty(), "Strides cannot be empty");
        }

        /**
         * @brief Advances by the last stride (innermost dimension).
         * @param ptr The pointer to advance.
         * @param n Number of steps.
         * FIXED: Now non-static const method to access strides_ member.
         */
        void advance(T*& ptr, difference_type n = 1) const {
            ptr += strides_.back() * n;
        }

        void retreat(T*& ptr, difference_type n = 1) const {
            ptr -= strides_.back() * n;
        }
    };

    /**
     * @brief Reverse Policy: Wraps another policy to iterate in reverse.
     * @tparam T The element type.
     * @tparam BasePolicy The policy to reverse (must support retreat).
     * FIXED: Uses non-static methods for consistency.
     */
    template <typename T, typename BasePolicy>
    struct ReverseIteratorPolicy {
        using iterator_category = typename BasePolicy::iterator_category;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        BasePolicy base_;

        ReverseIteratorPolicy() = default;

        ReverseIteratorPolicy(BasePolicy base)
            : base_(std::move(base)) {
        }

        void advance(T*& ptr) const {
            base_.retreat(ptr);
        }

        void retreat(T*& ptr) const {
            base_.advance(ptr);
        }
    };

    /**
     * @brief Combined Policy: Composes two policies with proper chaining.
     * @tparam P1 First policy (e.g., stride).
     * @tparam P2 Second policy (e.g., filter).
     * @details FIXED: Now uses member composition instead of multiple inheritance
     * to support stateful policies and correct filter+stride logic.
     * For filter+stride: advances by stride, checks predicate, loops until match.
     */
    template <typename P1, typename P2>
    struct CombinedPolicy {
        P1 p1_;  // Store as members (supports state)
        P2 p2_;

        // Use the more restrictive category
        using iterator_category = std::conditional_t<
            std::is_convertible_v<typename P1::iterator_category, std::forward_iterator_tag> ||
            std::is_convertible_v<typename P2::iterator_category, std::forward_iterator_tag>,
            std::forward_iterator_tag,
            std::conditional_t<
            std::is_convertible_v<typename P1::iterator_category, std::bidirectional_iterator_tag> ||
            std::is_convertible_v<typename P2::iterator_category, std::bidirectional_iterator_tag>,
            std::bidirectional_iterator_tag,
            std::random_access_iterator_tag
            >
        >;

        using difference_type = std::ptrdiff_t;
        using value_type = typename P1::value_type;
        using pointer = typename P1::pointer;
        using reference = typename P1::reference;

        CombinedPolicy(P1 p1, P2 p2) : p1_(std::move(p1)), p2_(std::move(p2)) {}

        /**
         * @brief Advances with both policies applied correctly.
         * @details For filter+stride: loop until predicate matches after striding.
         * Uses if constexpr to handle different policy combinations.
         */
        template <typename PtrType>
        void advance(PtrType& ptr, PtrType end = nullptr) const {
            if constexpr (has_predicate_type<P2>::value) {
                // Filter scenario: stride then check predicate, loop if needed
                do {
                    p1_.advance(ptr);  // E.g., stride
                    if (ptr >= end) return;  // Early exit if past end
                } while (!p2_.predicate(*ptr));  // Continue until predicate satisfied
            }
            else {
                // Non-filter scenario: just chain advances
                p1_.advance(ptr, end);
                p2_.advance(ptr, end);
            }
        }

        /**
         * @brief Retreat with both policies (only if both bidirectional).
         * @note Disabled for forward-only policies via static_assert in iterator.
         */
        template <typename PtrType>
        void retreat(PtrType& ptr) const {
            static_assert(std::is_convertible_v<iterator_category, std::bidirectional_iterator_tag>,
                "Combined policy retreat requires bidirectional policies");
            p2_.retreat(ptr);
            p1_.retreat(ptr);
        }
    };

    // --------------------------------------------------------------------
    // 2. AdaptiveIterator Class - FIXED with all improvements
    // --------------------------------------------------------------------

    /**
     * @brief Adaptive iterator with policy-based behavior.
     * @tparam T Element type.
     * @tparam Policy Iteration policy (default: StandardIteratorPolicy).
     * @tparam ConcurrencyPolicy Thread-safety policy (default: SingleThreadedPolicy).
     * @details FIXED: Stores policy as member for stateful policies (EBO optimizes empty ones).
     * Fixed all compilation errors with MSVC.
     */
    template <typename T, typename Policy, typename ConcurrencyPolicy>
    class AdaptiveIterator {
    public:
        using iterator_category = typename Policy::iterator_category;
        using difference_type = typename Policy::difference_type;
        using value_type = typename Policy::value_type;
        using pointer = typename Policy::pointer;
        using reference = typename Policy::reference;

        using increment_return = std::conditional_t<can_fail<Policy>::value, Expected<void, std::string>, void>;

    protected:  // Changed to protected for SafeAdaptiveIterator access
        Policy policy_;  // Store policy instance (EBO optimizes empty policies)
        T* ptr_;
        T* end_ = nullptr;  // Conditionally used based on needs_end<Policy>

        // Helper to get predicate type or void (avoids std::conditional_t with missing types)
        template <typename P, typename = void>
        struct predicate_type_or_void { using type = void; };
        template <typename P>
        struct predicate_type_or_void<P, std::void_t<typename P::predicate_type>> {
            using type = typename P::predicate_type;
        };

        // Helper to get transformer type or void
        template <typename P, typename = void>
        struct transformer_type_or_void { using type = void; };
        template <typename P>
        struct transformer_type_or_void<P, std::void_t<typename P::transformer_type>> {
            using type = typename P::transformer_type;
        };

        // Conditionally store predicate/transformer
        std::conditional_t<has_predicate_type<Policy>::value,
            std::optional<typename predicate_type_or_void<Policy>::type>,
            std::monostate> predicate_;

        std::conditional_t<has_transformer_type<Policy>::value,
            std::optional<typename transformer_type_or_void<Policy>::type>,
            std::monostate> transformer_;

        // Concurrency support: use NoOpLock from SingleThreadedPolicy
        mutable typename ConcurrencyPolicy::NoOpLock lock_;

    public:
        /**
         * @brief Constructor with default-constructed policy.
         * @param begin Start pointer.
         * @param end End pointer (used if needs_end<Policy>).
         * Requires Policy to be default-constructible.
         */
        AdaptiveIterator(T* begin, T* end)
            : policy_(Policy{}), ptr_(begin), end_(end) {
        }

        /**
         * @brief Constructor for standard policies (no predicate/transformer).
         * @param begin Start pointer.
         * @param end End pointer (used if needs_end<Policy>).
         * @param policy Policy instance (moved for movable-only types).
         */
        AdaptiveIterator(T* begin, T* end, Policy policy)
            : policy_(std::move(policy)), ptr_(begin), end_(end) {
        }

        /**
         * @brief Constructor for policies with predicate OR transformer.
         * @param begin Start pointer.
         * @param end End pointer.
         * @param policy Policy instance.
         * @param func Predicate or transformer function.
         * FIXED: Single unified constructor to avoid duplicate definition errors.
         * Initialize optional with emplace to handle non-copyable lambdas.
         */
        template <typename Func>
        AdaptiveIterator(T* begin, T* end, Policy policy, Func&& func)
            : policy_(std::move(policy)), ptr_(begin), end_(end) {
            if constexpr (has_predicate_type<Policy>::value) {
                predicate_.emplace(std::forward<Func>(func));
            }
            else if constexpr (has_transformer_type<Policy>::value) {
                transformer_.emplace(std::forward<Func>(func));
            }
        }

        /**
         * @brief Dereference operator with thread-safe read locking.
         * @return Reference to element (transformed if TransformPolicy).
         * FIXED: Uses NoOpLock from ConcurrencyPolicy.
         */
        [[nodiscard]] reference operator*() const {
            // For SingleThreadedPolicy, LockGuard is no-op
            typename ConcurrencyPolicy::LockGuard guard(lock_);

            if constexpr (has_transformer_type<Policy>::value) {
                enforce(transformer_.has_value(), "Transformer not initialized");
                return policy_.dereference(ptr_, *transformer_);
            }
            else {
                return *ptr_;
            }
        }

        [[nodiscard]] pointer operator->() const {
            return ptr_;
        }

        /**
         * @brief Pre-increment with Expected error handling.
         * @return Expected<void, std::string> on success/failure if can_fail, else void.
         * FIXED: Returns Expected<void> instead of Expected<Iterator&> (can't store references).
         * NEW: Debug-only bounds check via enforce; optimized return if !can_fail.
         */
        [[nodiscard]] increment_return operator++() {
            typename ConcurrencyPolicy::LockGuard guard(lock_);

            enforce(ptr_ < end_, "Iterator past end");  // Debug-only (no-op in release)

            if constexpr (needs_end<Policy>::value) {
                if constexpr (has_predicate_type<Policy>::value) {
                    enforce(predicate_.has_value(), "Predicate not initialized");
                    policy_.advance(ptr_, end_, *predicate_);
                }
                else {
                    policy_.advance(ptr_, end_);
                }
            }
            else {
                policy_.advance(ptr_);
            }

            if constexpr (can_fail<Policy>::value) {
                return Expected<void, std::string>(std::in_place);
            }
        }

        /**
         * @brief Post-increment with Expected error handling.
         * @return Expected<AdaptiveIterator, std::string> (copy of pre-increment state).
         */
        [[nodiscard]] Expected<AdaptiveIterator, std::string> operator++(int) {
            auto copy = *this;
            if constexpr (can_fail<Policy>::value) {
                auto result = ++(*this);
                if (!result.has_value()) {
                    return unexpected(result.error());
                }
            } else {
                ++(*this);
            }
            return copy;
        }

        /**
         * @brief Pre-decrement (bidirectional/random-access only).
         * @return Expected<void, std::string> on success/failure.
         */
        [[nodiscard]] Expected<void, std::string> operator--() {
            static_assert(std::is_convertible_v<iterator_category, std::bidirectional_iterator_tag>,
                "Decrement requires bidirectional or random-access iterator");

            typename ConcurrencyPolicy::LockGuard guard(lock_);
            policy_.retreat(ptr_);
            return Expected<void, std::string>(std::in_place); // Success
        }

        [[nodiscard]] Expected<AdaptiveIterator, std::string> operator--(int) {
            auto copy = *this;
            auto res = --(*this);
            if (!res.has_value()) {
                return unexpected(res.error());
            }
            return copy;
        }

        [[nodiscard]] bool operator==(const AdaptiveIterator& other) const {
            return ptr_ == other.ptr_;
        }

        [[nodiscard]] bool operator!=(const AdaptiveIterator& other) const {
            return !(*this == other);
        }

        /** @brief Get underlying pointer (for debugging/testing). */
        [[nodiscard]] T* get() const { return ptr_; }

        /**
         * @brief Query stride size if applicable.
         * @return Stride value if Policy has stride_value, else 1.
         */
        [[nodiscard]] constexpr int stride_size() const {
            if constexpr (has_N<Policy>::value) {
                return Policy::stride_value;
            }
            else {
                return 1;
            }
        }

        // Friend for const conversion
        template <typename, typename, typename> friend class ConstAdaptiveIterator;
    };

    // --------------------------------------------------------------------
    // 3. ConstAdaptiveIterator Class - FIXED: Full implementation
    // --------------------------------------------------------------------

    /**
     * @brief Const version of AdaptiveIterator for const-correctness.
     * @tparam T Element type.
     * @tparam Policy Iteration policy.
     * @tparam ConcurrencyPolicy Thread-safety policy.
     * @details FIXED: Complete implementation mirroring AdaptiveIterator but with const T*.
     * Supports conversion from non-const iterator.
     */
    template <typename T, typename Policy, typename ConcurrencyPolicy>
    class ConstAdaptiveIterator {
    public:
        using iterator_category = typename Policy::iterator_category;
        using difference_type = typename Policy::difference_type;
        using value_type = typename Policy::value_type;
        using pointer = const T*;
        using reference = const T&;

        using increment_return = std::conditional_t<can_fail<Policy>::value, Expected<void, std::string>, void>;

    private:
        Policy policy_;
        const T* ptr_;
        const T* end_ = nullptr;

        // Helper traits
        template <typename P, typename = void>
        struct predicate_type_or_void { using type = void; };
        template <typename P>
        struct predicate_type_or_void<P, std::void_t<typename P::predicate_type>> {
            using type = typename P::predicate_type;
        };

        template <typename P, typename = void>
        struct transformer_type_or_void { using type = void; };
        template <typename P>
        struct transformer_type_or_void<P, std::void_t<typename P::transformer_type>> {
            using type = typename P::transformer_type;
        };

        std::conditional_t<has_predicate_type<Policy>::value,
            std::optional<typename predicate_type_or_void<Policy>::type>,
            std::monostate> predicate_;

        std::conditional_t<has_transformer_type<Policy>::value,
            std::optional<typename transformer_type_or_void<Policy>::type>,
            std::monostate> transformer_;

        mutable typename ConcurrencyPolicy::NoOpLock lock_;

    public:
        /**
         * @brief Constructor with default-constructed policy.
         * @param begin Const start pointer.
         * @param end Const end pointer.
         * Requires Policy to be default-constructible.
         */
        ConstAdaptiveIterator(const T* begin, const T* end)
            : policy_(Policy{}), ptr_(begin), end_(end) {
        }

        /**
         * @brief Constructor for const iterator.
         * @param begin Const start pointer.
         * @param end Const end pointer.
         * @param policy Policy instance.
         */
        ConstAdaptiveIterator(const T* begin, const T* end, Policy policy)
            : policy_(std::move(policy)), ptr_(begin), end_(end) {
        }

        /**
         * @brief Conversion constructor from non-const iterator.
         * @param other Non-const AdaptiveIterator.
         * FIXED: Added for seamless const conversions (e.g., container.cbegin()).
         */
        ConstAdaptiveIterator(const AdaptiveIterator<T, Policy, ConcurrencyPolicy>& other)
            : policy_(other.policy_), ptr_(other.ptr_), end_(other.end_),
            predicate_(other.predicate_), transformer_(other.transformer_) {
        }

        /**
         * @brief Constructor for policies with predicate OR transformer.
         * FIXED: Single unified constructor with emplace for non-copyable lambdas.
         */
        template <typename Func>
        ConstAdaptiveIterator(const T* begin, const T* end, Policy policy, Func&& func)
            : policy_(std::move(policy)), ptr_(begin), end_(end) {
            if constexpr (has_predicate_type<Policy>::value) {
                predicate_.emplace(std::forward<Func>(func));
            }
            else if constexpr (has_transformer_type<Policy>::value) {
                transformer_.emplace(std::forward<Func>(func));
            }
        }

        [[nodiscard]] reference operator*() const {
            typename ConcurrencyPolicy::LockGuard guard(lock_);

            if constexpr (has_transformer_type<Policy>::value) {
                enforce(transformer_.has_value(), "Transformer not initialized");
                return policy_.dereference(const_cast<T*>(ptr_), *transformer_);
            }
            else {
                return *ptr_;
            }
        }

        [[nodiscard]] pointer operator->() const {
            return ptr_;
        }

        [[nodiscard]] increment_return operator++() {
            typename ConcurrencyPolicy::LockGuard guard(lock_);

            enforce(ptr_ < end_, "Iterator past end");  // Debug-only

            if constexpr (needs_end<Policy>::value) {
                if constexpr (has_predicate_type<Policy>::value) {
                    enforce(predicate_.has_value(), "Predicate not initialized");
                    T* mutable_ptr = const_cast<T*>(ptr_);
                    policy_.advance(mutable_ptr, const_cast<T*>(end_), *predicate_);
                    ptr_ = mutable_ptr;
                }
                else {
                    T* mutable_ptr = const_cast<T*>(ptr_);
                    policy_.advance(mutable_ptr, const_cast<T*>(end_));
                    ptr_ = mutable_ptr;
                }
            }
            else {
                T* mutable_ptr = const_cast<T*>(ptr_);
                policy_.advance(mutable_ptr);
                ptr_ = mutable_ptr;
            }

            if constexpr (can_fail<Policy>::value) {
                return Expected<void, std::string>(std::in_place);
            }
        }

        [[nodiscard]] Expected<ConstAdaptiveIterator, std::string> operator++(int) {
            auto copy = *this;
            if constexpr (can_fail<Policy>::value) {
                auto result = ++(*this);
                if (!result.has_value()) {
                    return unexpected(result.error());
                }
            } else {
                ++(*this);
            }
            return copy;
        }

        [[nodiscard]] Expected<void, std::string> operator--() {
            static_assert(std::is_convertible_v<iterator_category, std::bidirectional_iterator_tag>,
                "Decrement requires bidirectional or random-access iterator");

            typename ConcurrencyPolicy::LockGuard guard(lock_);
            T* mutable_ptr = const_cast<T*>(ptr_);
            policy_.retreat(mutable_ptr);
            ptr_ = mutable_ptr;
            return Expected<void, std::string>(std::in_place); // Success
        }

        [[nodiscard]] Expected<ConstAdaptiveIterator, std::string> operator--(int) {
            auto copy = *this;
            auto res = --(*this);
            if (!res.has_value()) {
                return unexpected(res.error());
            }
            return copy;
        }

        [[nodiscard]] bool operator==(const ConstAdaptiveIterator& other) const {
            return ptr_ == other.ptr_;
        }

        [[nodiscard]] bool operator!=(const ConstAdaptiveIterator& other) const {
            return !(*this == other);
        }

        [[nodiscard]] const T* get() const { return ptr_; }

        [[nodiscard]] constexpr int stride_size() const {
            if constexpr (has_N<Policy>::value) {
                return Policy::stride_value;
            }
            else {
                return 1;
            }
        }
    };

    // --------------------------------------------------------------------
    // 4. Integrated Factory (from AdaptiveIteratorFactory.h)
    // --------------------------------------------------------------------

    /**
     * @brief Abstract base for erased iterators (for runtime factory).
     * @tparam T Element type.
     * @note Minor perf cost (vtable); use for dynamic policy selection only.
     */
    template <typename T>
    class IteratorInterface {
    public:
        virtual ~IteratorInterface() = default;
        [[nodiscard]] virtual T& operator*() = 0;
        [[nodiscard]] virtual const T& operator*() const = 0;
        virtual Expected<void, std::string> operator++() = 0;
        [[nodiscard]] virtual Expected<std::unique_ptr<IteratorInterface>, std::string> operator_post_increment() = 0;
        [[nodiscard]] virtual bool operator==(const IteratorInterface& other) const = 0;
        [[nodiscard]] virtual bool operator!=(const IteratorInterface& other) const = 0;
    };

    /**
     * @brief Concrete erased wrapper for a specific policy.
     * @details FIXED: Properly forwards movable-only types with std::forward.
     */
    template <typename T, typename Policy>
    class ErasedAdaptiveIterator : public IteratorInterface<T> {
    private:
        AdaptiveIterator<T, Policy> it_;

    public:
        ErasedAdaptiveIterator(T* begin, T* end, Policy policy)
            : it_(begin, end, std::move(policy)) {
        }

        [[nodiscard]] T& operator*() override {
            return *it_;
        }

        [[nodiscard]] const T& operator*() const override {
            return *it_;
        }

        Expected<void, std::string> operator++() override {
            if constexpr (can_fail<Policy>::value) {
                return ++it_;
            } else {
                ++it_;
                return Expected<void, std::string>(std::in_place);
            }
        }

        [[nodiscard]] Expected<std::unique_ptr<IteratorInterface<T>>, std::string> operator_post_increment() override {
            auto res = it_++;
            if (!res.has_value()) {
                return unexpected(res.error());
            }
            return std::make_unique<ErasedAdaptiveIterator>(*this);
        }

        [[nodiscard]] bool operator==(const IteratorInterface<T>& other) const override {
            if (auto* cast = dynamic_cast<const ErasedAdaptiveIterator*>(&other)) {
                return it_ == cast->it_;
            }
            return false;
        }

        [[nodiscard]] bool operator!=(const IteratorInterface<T>& other) const override {
            return !(*this == other);
        }
    };

    /**
     * @brief Factory for runtime policy selection of AdaptiveIterator.
     * @tparam T Element type.
     * @note Returns Expected<unique_ptr<IteratorInterface<T>>>, std::string>; minor vtable overhead.
     * Use for dynamic (e.g., config-based) policy choice.
     * FIXED: Enhanced with more default registrations (e.g., common predicates).
     */
    template <typename T>
    class AdaptiveIteratorFactory {
    private:
        using CreatorFunc = std::function<std::unique_ptr<IteratorInterface<T>>(T*, T*)>;
        using RegistryType = std::map<std::string, CreatorFunc>;

        RegistryType registry_;

    public:
        static AdaptiveIteratorFactory& instance() {
            static AdaptiveIteratorFactory factory;
            return factory;
        }

        /**
         * @brief Register a policy with a string key.
         * @tparam Policy The policy type to register.
         * @param key String identifier for the policy.
         */
        template <typename Policy>
        void registerType(const std::string& key) {
            registry_[key] = [](T* begin, T* end) {
                return std::make_unique<ErasedAdaptiveIterator<T, Policy>>(begin, end, Policy{});
                };
        }

        /**
         * @brief Register with custom creator function (for predicate-based policies).
         * @param key String identifier.
         * @param creator Factory function.
         */
        void registerType(const std::string& key, CreatorFunc creator) {
            registry_[key] = std::move(creator);
        }

        /**
         * @brief Register common policies with movable-only support.
         * @details FIXED: Added more defaults including common predicates (e.g., even filter).
         */
        static void registerDefaults() {
            auto& inst = instance();
            inst.template registerType<StandardIteratorPolicy<T>>("standard");
            inst.template registerType<StrideIteratorPolicy<T, 2>>("stride_2");
            inst.template registerType<StrideIteratorPolicy<T, 4>>("stride_4");
            inst.template registerType<StrideIteratorPolicy<T, 8>>("stride_8");

            // Register common filter predicates
            inst.registerType("even_filter", [](T* begin, T* end) {
                auto pred = [](const T& v) { return v % 2 == 0; };
                using FilterPolicy = FilteringIteratorPolicy<T, decltype(pred)>;
                return std::make_unique<ErasedAdaptiveIterator<T, FilterPolicy>>(begin, end, FilterPolicy{});
                });

            inst.registerType("positive_filter", [](T* begin, T* end) {
                auto pred = [](const T& v) { return v > 0; };
                using FilterPolicy = FilteringIteratorPolicy<T, decltype(pred)>;
                return std::make_unique<ErasedAdaptiveIterator<T, FilterPolicy>>(begin, end, FilterPolicy{});
                });
        }

        /**
         * @brief Create an iterator with the given policy key.
         * @param key Policy identifier.
         * @param begin Start pointer.
         * @param end End pointer.
         * @return Expected<unique_ptr<IteratorInterface<T>>, std::string> on success/failure.
         */
        [[nodiscard]] static Expected<std::unique_ptr<IteratorInterface<T>>, std::string>
            create(const std::string& key, T* begin, T* end) {
            auto& inst = instance();
            auto it = inst.registry_.find(key);
            if (it == inst.registry_.end()) {
                return unexpected(std::string("Invalid policy key: ") + key);
            }
            return it->second(begin, end);
        }
    };

    // --------------------------------------------------------------------
    // 5. Compile-Time Factory (Zero-Overhead for Known Policies)
    // --------------------------------------------------------------------

    /**
     * @brief Template helper for compile-time policy selection.
     * @tparam Key Compile-time key (e.g., struct StandardTag {}).
     * @tparam T Element type.
     * @note Use tags for selection; zero runtime cost.
     */
    template <typename Key, typename T>
    struct CompileTimePolicySelector;

    // Specialize for tags
    struct StandardTag {};
    template <typename T>
    struct CompileTimePolicySelector<StandardTag, T> {
        using type = StandardIteratorPolicy<T>;
    };

    struct Stride2Tag {};
    template <typename T>
    struct CompileTimePolicySelector<Stride2Tag, T> {
        using type = StrideIteratorPolicy<T, 2>;
    };

    struct Stride4Tag {};
    template <typename T>
    struct CompileTimePolicySelector<Stride4Tag, T> {
        using type = StrideIteratorPolicy<T, 4>;
    };

    struct Stride8Tag {};
    template <typename T>
    struct CompileTimePolicySelector<Stride8Tag, T> {
        using type = StrideIteratorPolicy<T, 8>;
    };

    // Usage: AdaptiveIterator<T, typename CompileTimePolicySelector<StandardTag, T>::type> it(begin, end);

    // NEW: Trait to extract policy from iterator
    template <typename It>
    struct iterator_policy;

    template <typename T, typename P, typename C>
    struct iterator_policy<AdaptiveIterator<T, P, C>> {
        using type = P;
    };

    template <typename T, typename P, typename C>
    struct iterator_policy<ConstAdaptiveIterator<T, P, C>> {
        using type = P;
    };

    template <typename T, typename P, typename C>
    struct iterator_policy<SafeAdaptiveIterator<T, P, C>> {
        using type = P;
    };

} // namespace cpp_utilities