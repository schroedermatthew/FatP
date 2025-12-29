/**
 * @file PolicyIterator.h
 * @brief Policy-based iterator with customizable traversal strategies.
 *
 * @layer Policy
 *
 * Provides a compile-time configurable iterator that delegates advancing and
 * dereferencing to policy classes. Supports standard, stride, filtering,
 * transform, and tensor iteration with static dispatch for zero runtime overhead.
 *
 * @note Precondition checks are debug-only via enforce; release builds have zero overhead.
 * @note Thread-safety: Not internally synchronized; use external synchronization.
 *
 * @see enforce.h for contract checking.
 * @see TensorStridePolicy.h for multi-dimensional array traversal.
 */

#pragma once
#include <cstddef>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include "enforce.h"

namespace fat_p::iterator {

// --------------------------------------------------------------------
// Policy Traits
// --------------------------------------------------------------------

template <typename P, typename = void>
struct has_stride : std::false_type {};

template <typename P>
struct has_stride<P, std::void_t<decltype(P::kStrideValue)>> : std::true_type {};

template <typename P, typename = void>
struct has_predicate : std::false_type {};

template <typename P>
struct has_predicate<P, std::void_t<typename P::predicate_type>> : std::true_type {};

template <typename P, typename = void>
struct has_transformer : std::false_type {};

template <typename P>
struct has_transformer<P, std::void_t<typename P::transformer_type>> : std::true_type {};

// Trait to detect policies that need end-clamping (have kNeedsEndClamp = true)
template <typename P, typename = void>
struct needs_end_clamp : std::false_type {};

template <typename P>
struct needs_end_clamp<P, std::void_t<decltype(P::kNeedsEndClamp)>> 
    : std::bool_constant<P::kNeedsEndClamp> {};

// Trait to detect tensor policies (position-based iteration)
template <typename P, typename = void>
struct is_tensor_policy : std::false_type {};

template <typename P>
struct is_tensor_policy<P, std::void_t<decltype(P::kIsTensorPolicy)>>
    : std::bool_constant<P::kIsTensorPolicy> {};

// Trait to detect policies requiring a functor (predicate or transformer)
template <typename P>
struct requires_functor : std::bool_constant<has_predicate<P>::value || has_transformer<P>::value> {};

// --------------------------------------------------------------------
// Policies
// --------------------------------------------------------------------

/**
 * @brief Standard sequential iteration with zero overhead.
 * @tparam T Element type.
 */
template <typename T>
struct StandardPolicy {
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    void advance(T*& ptr) const { ++ptr; }
    void retreat(T*& ptr) const { --ptr; }
};

/**
 * @brief Stride iteration advancing N elements at a time.
 * @tparam T Element type.
 * @tparam N Stride size (must be > 0).
 *
 * @note Uses bidirectional_iterator_tag because random_access would require
 *       operator+, operator-, operator[], etc. which aren't implemented.
 */
template <typename T, int N>
struct StridePolicy {
    static_assert(N > 0, "Stride must be positive");

    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    static constexpr int kStrideValue = N;
    static constexpr bool kNeedsEndClamp = true;  ///< Signals PolicyIterator to use advance(ptr, end)

    void advance(T*& ptr) const { ptr += N; }
    
    /// Advance with bounds checking to avoid UB from ptr arithmetic past end.
    void advance(T*& ptr, T* end) const {
        // Check distance BEFORE advancing to avoid UB
        if (end - ptr <= N) {
            ptr = end;
        } else {
            ptr += N;
        }
    }
    
    void retreat(T*& ptr) const { ptr -= N; }
};

/**
 * @brief Filtering iteration that skips elements not matching predicate.
 * @tparam T Element type.
 * @tparam Predicate Callable with signature bool(const T&).
 */
template <typename T, typename Predicate>
struct FilterPolicy {
    using predicate_type = Predicate;
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    void advance(T*& ptr, T* end, const Predicate& pred) const {
        ++ptr;
        while (ptr < end && !pred(*ptr)) ++ptr;
    }
};

/**
 * @brief Transform iteration applying a function on dereference.
 * @tparam T Underlying element type.
 * @tparam Transformer Callable with signature U(const T&).
 *
 * @note operator* returns the transformed value (by value).
 * @note operator-> returns pointer to the underlying T, NOT the transformed value.
 *       This is intentional: arrow provides access to the original element,
 *       while dereference provides the transformed view.
 */
template <typename T, typename Transformer>
struct TransformPolicy {
    using transformer_type = Transformer;
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using result_type = std::invoke_result_t<Transformer, const T&>;
    using value_type = std::remove_cv_t<std::remove_reference_t<result_type>>;
    using reference = value_type;
    using pointer = const T*;

    void advance(T*& ptr) const { ++ptr; }
    void retreat(T*& ptr) const { --ptr; }

    [[nodiscard]] reference dereference(const T* ptr, const Transformer& fn) const {
        return fn(*ptr);
    }
};

// --------------------------------------------------------------------
// PolicyIterator
// --------------------------------------------------------------------

/**
 * @brief Policy-based iterator with compile-time configurable behavior.
 * @tparam T Element type.
 * @tparam Policy Iteration policy (default: StandardPolicy<T>).
 *
 * Constructor availability:
 * - 2-arg (begin, end): Only for policies NOT requiring a functor
 * - 3-arg (begin, end, policy): Only for policies NOT requiring a functor
 * - 4-arg (begin, end, policy, func): Only for policies requiring a functor
 *
 * For tensor policies, comparison uses position rather than pointer.
 */
template <typename T, typename Policy = StandardPolicy<T>>
class PolicyIterator {
public:
    using iterator_category = typename Policy::iterator_category;
    using difference_type = typename Policy::difference_type;
    using value_type = typename Policy::value_type;
    using pointer = typename Policy::pointer;
    using reference = typename Policy::reference;

private:
    Policy mPolicy;
    T* mPtr;
    T* mEnd;
    T* mBase;  // Base pointer for tensor policies

    template <typename P, typename = void>
    struct predicate_or_monostate { using type = std::monostate; };
    template <typename P>
    struct predicate_or_monostate<P, std::void_t<typename P::predicate_type>> {
        using type = std::optional<typename P::predicate_type>;
    };

    template <typename P, typename = void>
    struct transformer_or_monostate { using type = std::monostate; };
    template <typename P>
    struct transformer_or_monostate<P, std::void_t<typename P::transformer_type>> {
        using type = std::optional<typename P::transformer_type>;
    };

    typename predicate_or_monostate<Policy>::type mPredicate;
    typename transformer_or_monostate<Policy>::type mTransformer;

    // Update mPtr from tensor policy offset
    void syncPtrFromTensor() {
        if constexpr (is_tensor_policy<Policy>::value) {
            if (mPolicy.atEnd()) {
                mPtr = mEnd;
            } else {
                mPtr = mBase + mPolicy.currentOffset();
            }
        }
    }

public:
    // ----------------------------------------------------------------
    // Constructors: 2-arg and 3-arg disabled for functor-requiring policies
    // ----------------------------------------------------------------

    /// 2-arg constructor: Only for policies that don't require a functor
    template <typename P = Policy, 
              std::enable_if_t<!requires_functor<P>::value && !is_tensor_policy<P>::value, int> = 0>
    PolicyIterator(T* begin, T* end)
        : mPolicy{}, mPtr(begin), mEnd(end), mBase(begin) {}

    /// 3-arg constructor: Only for policies that don't require a functor
    template <typename P = Policy,
              std::enable_if_t<!requires_functor<P>::value && !is_tensor_policy<P>::value, int> = 0>
    PolicyIterator(T* begin, T* end, Policy policy)
        : mPolicy(std::move(policy)), mPtr(begin), mEnd(end), mBase(begin) {}

    /// 3-arg constructor for tensor policies (no functor needed)
    template <typename P = Policy,
              std::enable_if_t<is_tensor_policy<P>::value, int> = 0>
    PolicyIterator(T* base, T* end, Policy policy)
        : mPolicy(std::move(policy)), mPtr(base), mEnd(end), mBase(base) {
        syncPtrFromTensor();
    }

    /// 4-arg constructor: Only for policies that require a functor (predicate/transform)
    template <typename Func, typename P = Policy,
              std::enable_if_t<requires_functor<P>::value, int> = 0>
    PolicyIterator(T* begin, T* end, Policy policy, Func&& func)
        : mPolicy(std::move(policy)), mPtr(begin), mEnd(end), mBase(begin) {
        if constexpr (has_predicate<Policy>::value) {
            mPredicate.emplace(std::forward<Func>(func));
            // Advance to first matching element
            while (mPtr < mEnd && !(*mPredicate)(*mPtr)) ++mPtr;
        }
        else if constexpr (has_transformer<Policy>::value) {
            mTransformer.emplace(std::forward<Func>(func));
        }
    }

    /// Create an end iterator for tensor policies
    template <typename P = Policy,
              std::enable_if_t<is_tensor_policy<P>::value, int> = 0>
    static PolicyIterator makeEnd(T* base, T* end, Policy policy) {
        PolicyIterator it(base, end, std::move(policy));
        it.mPolicy.setToEnd();
        it.mPtr = end;
        return it;
    }

    // ----------------------------------------------------------------
    // Element access
    // ----------------------------------------------------------------

    [[nodiscard]] reference operator*() const {
        if constexpr (has_transformer<Policy>::value) {
            enforce(mTransformer.has_value(), "Transformer not initialized");
            return mPolicy.dereference(mPtr, *mTransformer);
        }
        else if constexpr (is_tensor_policy<Policy>::value) {
            enforce(!mPolicy.atEnd(), "Cannot dereference end iterator");
            return *mPtr;
        }
        else {
            return *mPtr;
        }
    }

    [[nodiscard]] pointer operator->() const { 
        if constexpr (is_tensor_policy<Policy>::value) {
            enforce(!mPolicy.atEnd(), "Cannot dereference end iterator");
        }
        return mPtr; 
    }

    // ----------------------------------------------------------------
    // Increment/Decrement
    // ----------------------------------------------------------------

    PolicyIterator& operator++() {
        if constexpr (is_tensor_policy<Policy>::value) {
            enforce(!mPolicy.atEnd(), "Iterator past end");
            mPolicy.advance();
            syncPtrFromTensor();
        }
        else {
            enforce(mPtr < mEnd, "Iterator past end");
            if constexpr (has_predicate<Policy>::value) {
                enforce(mPredicate.has_value(), "Predicate not initialized");
                mPolicy.advance(mPtr, mEnd, *mPredicate);
            }
            else if constexpr (has_stride<Policy>::value || needs_end_clamp<Policy>::value) {
                mPolicy.advance(mPtr, mEnd);
            }
            else {
                mPolicy.advance(mPtr);
            }
        }
        return *this;
    }

    PolicyIterator operator++(int) {
        auto copy = *this;
        ++(*this);
        return copy;
    }

    PolicyIterator& operator--() {
        static_assert(std::is_convertible_v<iterator_category, std::bidirectional_iterator_tag>,
            "Decrement requires bidirectional iterator");
        if constexpr (is_tensor_policy<Policy>::value) {
            mPolicy.retreat();
            syncPtrFromTensor();
        }
        else {
            mPolicy.retreat(mPtr);
        }
        return *this;
    }

    PolicyIterator operator--(int) {
        auto copy = *this;
        --(*this);
        return copy;
    }

    // ----------------------------------------------------------------
    // Comparison: tensor policies compare position, others compare pointer
    // ----------------------------------------------------------------

    [[nodiscard]] bool operator==(const PolicyIterator& other) const {
        if constexpr (is_tensor_policy<Policy>::value) {
            return mPolicy.position() == other.mPolicy.position();
        }
        else {
            return mPtr == other.mPtr;
        }
    }

    [[nodiscard]] bool operator!=(const PolicyIterator& other) const {
        return !(*this == other);
    }

    // ----------------------------------------------------------------
    // Accessors
    // ----------------------------------------------------------------

    /// Returns the underlying pointer (respects Policy::pointer type)
    [[nodiscard]] pointer get() const { return mPtr; }

    [[nodiscard]] constexpr int strideSize() const {
        if constexpr (has_stride<Policy>::value) return Policy::kStrideValue;
        else return 1;
    }

    /// Access to policy (for tensor policies to query position/shape)
    [[nodiscard]] const Policy& policy() const { return mPolicy; }
};

} // namespace fat_p::iterator
