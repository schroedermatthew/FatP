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
// Policy Traits (implementation detail - not part of public API)
// --------------------------------------------------------------------

namespace detail {

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

// Trait to detect policies with setToEnd(pointer&, pointer, pointer) for end iterator initialization
template <typename P, typename = void>
struct has_set_to_end : std::false_type {};

template <typename P>
struct has_set_to_end<P, std::void_t<
    decltype(std::declval<P&>().setToEnd(
        std::declval<typename P::pointer&>(),
        std::declval<typename P::pointer>(),
        std::declval<typename P::pointer>()))
>> : std::true_type {};

} // namespace detail

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
 * @note Forward-only iterator. Bidirectional would require tracking which
 *       positions were actually visited, because `--end` on a misaligned
 *       range yields a position never reached during forward iteration.
 *       Example: size=10, stride=4 visits {0,4,8} but --end would give 6.
 */
template <typename T, int N>
struct StridePolicy {
    static_assert(N > 0, "Stride must be positive");

    using iterator_category = std::forward_iterator_tag;
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
 * CONSTRUCTION:
 * Use the static factory methods begin() and end() to create iterators.
 * This ensures proper bounds tracking for all iterator operations.
 *
 * @code
 * // Standard iteration
 * auto b = PolicyIterator<int>::begin(data, data + n);
 * auto e = PolicyIterator<int>::end(data, data + n);
 * 
 * // With stride policy
 * using Iter = PolicyIterator<int, StridePolicy<int, 2>>;
 * auto b = Iter::begin(data, data + n);
 * auto e = Iter::end(data, data + n);
 * 
 * // With functor (filter/transform)
 * auto pred = [](const int& x) { return x > 0; };
 * using P = FilterPolicy<int, decltype(pred)>;
 * auto b = PolicyIterator<int, P>::begin(data, data + n, P{}, pred);
 * auto e = PolicyIterator<int, P>::end(data, data + n, P{}, pred);
 * @endcode
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
    T* mBase;

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

    /// Update mPtr from tensor policy offset.
    /// CRITICAL: Compare INTEGER quantities before ANY pointer arithmetic to avoid UB.
    void syncPtrFromTensor() {
        if constexpr (detail::is_tensor_policy<Policy>::value) {
            if (mPolicy.atEnd()) {
                mPtr = mEnd;
            } else {
                auto offset = mPolicy.currentOffset();
                // Compute span as integer - NO pointer arithmetic yet
                // [[maybe_unused]] because enforce() compiles out in release
                [[maybe_unused]] auto span = mEnd - mBase;
                // Integer comparisons - safe, no UB possible
                enforce(offset >= 0, "Tensor offset cannot be negative");
                enforce(offset < span, "Tensor offset exceeds buffer bounds");
                // NOW safe to form pointer
                mPtr = mBase + offset;
            }
        }
    }

    // ----------------------------------------------------------------
    // Private constructors - use static factories instead
    // ----------------------------------------------------------------

    /// Core constructor for non-functor policies
    PolicyIterator(T* base, T* end, T* ptr, Policy policy)
        : mPolicy(std::move(policy)), mPtr(ptr), mEnd(end), mBase(base) {}

    /// Core constructor for functor policies
    template <typename Func>
    PolicyIterator(T* base, T* end, T* ptr, Policy policy, Func&& func)
        : mPolicy(std::move(policy)), mPtr(ptr), mEnd(end), mBase(base) {
        if constexpr (detail::has_predicate<Policy>::value) {
            mPredicate.emplace(std::forward<Func>(func));
        }
        else if constexpr (detail::has_transformer<Policy>::value) {
            mTransformer.emplace(std::forward<Func>(func));
        }
    }

public:
    // ----------------------------------------------------------------
    // Static Factories: The canonical way to construct iterators
    // ----------------------------------------------------------------

    // --- Standard policies (non-functor, non-tensor) ---

    /// Create a begin iterator for standard policies
    template <typename P = Policy,
              std::enable_if_t<!detail::requires_functor<P>::value && !detail::is_tensor_policy<P>::value, int> = 0>
    [[nodiscard]] static PolicyIterator begin(T* base, T* end) {
        return PolicyIterator(base, end, base, Policy{});
    }

    /// Create a begin iterator for standard policies with explicit policy
    template <typename P = Policy,
              std::enable_if_t<!detail::requires_functor<P>::value && !detail::is_tensor_policy<P>::value, int> = 0>
    [[nodiscard]] static PolicyIterator begin(T* base, T* end, Policy policy) {
        return PolicyIterator(base, end, base, std::move(policy));
    }

    /// Create an end iterator for standard policies
    template <typename P = Policy,
              std::enable_if_t<!detail::requires_functor<P>::value && !detail::is_tensor_policy<P>::value, int> = 0>
    [[nodiscard]] static PolicyIterator end(T* base, T* end) {
        Policy policy{};
        T* ptr = end;
        if constexpr (detail::has_set_to_end<Policy>::value) {
            policy.setToEnd(ptr, base, end);
        }
        return PolicyIterator(base, end, ptr, std::move(policy));
    }

    /// Create an end iterator for standard policies with explicit policy
    template <typename P = Policy,
              std::enable_if_t<!detail::requires_functor<P>::value && !detail::is_tensor_policy<P>::value, int> = 0>
    [[nodiscard]] static PolicyIterator end(T* base, T* end, Policy policy) {
        T* ptr = end;
        if constexpr (detail::has_set_to_end<Policy>::value) {
            policy.setToEnd(ptr, base, end);
        }
        return PolicyIterator(base, end, ptr, std::move(policy));
    }

    // --- Filter policies ---

    /// Create a begin iterator for filter policies
    template <typename Func, typename P = Policy,
              std::enable_if_t<detail::has_predicate<P>::value, int> = 0>
    [[nodiscard]] static PolicyIterator begin(T* base, T* end, Policy policy, Func&& func) {
        PolicyIterator it(base, end, base, std::move(policy), std::forward<Func>(func));
        // Advance to first matching element
        while (it.mPtr < it.mEnd && !(*it.mPredicate)(*it.mPtr)) {
            ++it.mPtr;
        }
        return it;
    }

    /// Create an end iterator for filter policies
    template <typename Func, typename P = Policy,
              std::enable_if_t<detail::has_predicate<P>::value, int> = 0>
    [[nodiscard]] static PolicyIterator end(T* base, T* end, Policy policy, Func&& func) {
        return PolicyIterator(base, end, end, std::move(policy), std::forward<Func>(func));
    }

    // --- Transform policies ---

    /// Create a begin iterator for transform policies
    template <typename Func, typename P = Policy,
              std::enable_if_t<detail::has_transformer<P>::value, int> = 0>
    [[nodiscard]] static PolicyIterator begin(T* base, T* end, Policy policy, Func&& func) {
        return PolicyIterator(base, end, base, std::move(policy), std::forward<Func>(func));
    }

    /// Create an end iterator for transform policies
    template <typename Func, typename P = Policy,
              std::enable_if_t<detail::has_transformer<P>::value, int> = 0>
    [[nodiscard]] static PolicyIterator end(T* base, T* end, Policy policy, Func&& func) {
        return PolicyIterator(base, end, end, std::move(policy), std::forward<Func>(func));
    }

    // --- Tensor policies ---

    /// Create a begin iterator for tensor policies
    template <typename P = Policy,
              std::enable_if_t<detail::is_tensor_policy<P>::value, int> = 0>
    [[nodiscard]] static PolicyIterator begin(T* base, T* end, Policy policy) {
        PolicyIterator it(base, end, base, std::move(policy));
        it.syncPtrFromTensor();
        return it;
    }

    /// Create an end iterator for tensor policies
    template <typename P = Policy,
              std::enable_if_t<detail::is_tensor_policy<P>::value, int> = 0>
    [[nodiscard]] static PolicyIterator end(T* base, T* end, Policy policy) {
        PolicyIterator it(base, end, end, std::move(policy));
        it.mPolicy.setToEnd();
        return it;
    }

    // ----------------------------------------------------------------
    // Element access
    // ----------------------------------------------------------------

    [[nodiscard]] reference operator*() const {
        if constexpr (detail::has_transformer<Policy>::value) {
            enforce(mTransformer.has_value(), "Transformer not initialized");
            enforce(mPtr < mEnd, "Cannot dereference end iterator");
            return mPolicy.dereference(mPtr, *mTransformer);
        }
        else if constexpr (detail::is_tensor_policy<Policy>::value) {
            enforce(!mPolicy.atEnd(), "Cannot dereference end iterator");
            return *mPtr;
        }
        else {
            enforce(mPtr < mEnd, "Cannot dereference end iterator");
            return *mPtr;
        }
    }

    [[nodiscard]] pointer operator->() const { 
        if constexpr (detail::is_tensor_policy<Policy>::value) {
            enforce(!mPolicy.atEnd(), "Cannot dereference end iterator");
        }
        else {
            enforce(mPtr < mEnd, "Cannot dereference end iterator");
        }
        return mPtr; 
    }

    // ----------------------------------------------------------------
    // Increment/Decrement
    // ----------------------------------------------------------------

    PolicyIterator& operator++() {
        if constexpr (detail::is_tensor_policy<Policy>::value) {
            enforce(!mPolicy.atEnd(), "Iterator past end");
            mPolicy.advance();
            syncPtrFromTensor();
        }
        else {
            enforce(mPtr < mEnd, "Iterator past end");
            if constexpr (detail::has_predicate<Policy>::value) {
                enforce(mPredicate.has_value(), "Predicate not initialized");
                mPolicy.advance(mPtr, mEnd, *mPredicate);
            }
            else if constexpr (detail::has_stride<Policy>::value || detail::needs_end_clamp<Policy>::value) {
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
        if constexpr (detail::is_tensor_policy<Policy>::value) {
            mPolicy.retreat();
            syncPtrFromTensor();
        }
        else {
            enforce(mPtr > mBase, "Cannot retreat before begin");
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
        if constexpr (detail::is_tensor_policy<Policy>::value) {
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
        if constexpr (detail::has_stride<Policy>::value) return Policy::kStrideValue;
        else return 1;
    }

    /// Access to policy (for tensor policies to query position/shape)
    [[nodiscard]] const Policy& policy() const { return mPolicy; }
};

} // namespace fat_p::iterator
