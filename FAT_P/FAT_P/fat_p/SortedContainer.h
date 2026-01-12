/**
 * @file SortedContainer.h
 * @layer Domain
 * @brief A policy-based sorted vector container that maintains order on inserts.
 *
 * @details Uses Policy-Based Design for uniqueness, comparison, allocation, 
 * concurrency, and logging. Supports batch inserts with sorting. Read-only 
 * iterators to preserve order.
 * 
 * Integrates with library's DbC (enforce.h), Expected for error handling, and 
 * EqualityComparisons for fuzzy compares. Conditional thread-safety via 
 * ConcurrencyPolicy from ConcurrencyPolicies.h (SingleThreadedPolicy, 
 * MutexSynchronizationPolicy, SharedMutexPolicy, SpinlockSynchronizationPolicy, etc.).
 * 
 * Extensible with new policies (e.g., LoggingPolicy for inserts).
 * 
 * Features: 
 * - Erase support with erase_if() and erase_range()
 * - Binary search: lower/upper_bound, equal_range, contains, count
 * - Reverse iterators
 * - Fuzzy uniqueness with configurable epsilon
 * - Auto-invariant checks in debug builds
 * - const T inserts, movable-only T support
 * - reserve/capacity/clear/shrink_to_fit methods
 * - Thread-safe forEach() iteration
 * 
 * Optimized: Reserve aggressively; insertion sort for small batches (<16); 
 * stable_sort for large batches.
 * 
 * C++17 compliant; header-only; no external deps (guards optional 
 * <shared_mutex>/<atomic>).
 *
 * @note Invariant checks are debug-only (via enforce); release has zero overhead.
 * @note Supports movable-only T via move inserts; tests with floats for fuzzy.
 * @note Exposes internal vector with warnings (Modifying may break invariant).
 * @note All synchronization policies are imported from ConcurrencyPolicies.h.
 * 
 * @section thread_safety Thread Safety
 * 
 * When using a concurrent policy (MutexSynchronizationPolicy, etc.):
 * 
 * - Individual method calls are atomic
 * - Iterators returned from begin()/end()/find()/lower_bound()/upper_bound()
 *   are NOT protected after return - use forEach() or withInternalContainer()
 * - Compound operations require external synchronization
 * 
 * Safe patterns:
 * @code
 * container.forEach([](const T& elem) { process(elem); });
 * auto copy = container.toVector();  // Safe snapshot
 * auto found = container.findCopy(value);  // Returns std::optional<T>
 * bool exists = container.contains(value);  // Thread-safe check
 * @endcode
 * 
 * Unsafe patterns:
 * @code
 * for (auto it = container.begin(); it != container.end(); ++it) // UNSAFE!
 *     process(*it);  // Iterator may be invalidated by another thread
 * @endcode
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: SortedContainer
  file_role: public_header
  path: fat_p/SortedContainer.h
  namespace: fat_p
  layer: Containers
  summary: "Public header for SortedContainer."
  api_stability: in_work
  related:
    docs_search: "SortedContainer"
    tests:
      - tests/test_FatPTypeTraits.cpp
      - tests/test_SortedContainer.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 2
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#if !defined(FATP_USE_SHARED_MUTEX)
#define FATP_USE_SHARED_MUTEX 1 // Enable by default; undef to disable
#endif
#if !defined(FATP_USE_ATOMIC)
#define FATP_USE_ATOMIC 1 // Enable by default
#endif

#include <algorithm>
#include <vector>
#include <deque>
#include <sstream>
#include <stdexcept>
#include <functional>
#include <type_traits>
#include <mutex>
#include <iterator>  // FIX: Missing header (std::distance, std::reverse_iterator)
#include <memory>    // FIX: Missing header (std::allocator)

#if FATP_USE_SHARED_MUTEX
#include <shared_mutex>
#endif
#if FATP_USE_ATOMIC
#include <atomic>
#endif

#include <optional>
#include <tuple>
#include <utility>

#include "CheckedArithmeticInt.h"
#include "ComparisonTolerances.h"
#include "ConcurrencyPolicies.h"
#include "enforce.h"
#include "EnforcedInit.h"
#include "EqualityComparisons.h"
#include "DiagnosticLogger_Core.h"
#include "Expected.h"
#include "FatPTypeTraits.h"  // For has_shrink_to_fit_v, is_sorted_container
#include "ScopeGuardPolicies.h"
#include "TypeTraits.h"       // For has_reserve_v

namespace fat_p {

// =============================================================================
// Backend Policies (FIX: Moved inside fat_p namespace)
// =============================================================================

/**
 * @brief Backend policy using std::vector for storage.
 * @tparam T Element type.
 * @tparam Allocator Allocator type.
 */
template <typename T, typename Allocator>
struct VectorBackendPolicy {
    using type = std::vector<T, Allocator>;
};

/**
 * @brief Backend policy using std::deque for storage.
 * @tparam T Element type.
 * @tparam Allocator Allocator type.
 */
template <typename T, typename Allocator>
struct DequeBackendPolicy {
    using type = std::deque<T, Allocator>;
};

// =============================================================================
// Fuzzy Policy Trait Detection (FIX: Uses member typedef detection)
// =============================================================================

// NOTE: has_shrink_to_fit_v provided by FatPTypeTraits.h, has_reserve_v by TypeTraits.h

/**
 * @brief Trait to detect fuzzy unique policies.
 * 
 * Uses member typedef detection to support wrapped policies like
 * LoggingUniquePolicy<FuzzyUniquePolicy<>>.
 */
template <typename Policy, typename = void>
struct is_fuzzy_unique_policy : std::false_type {};

template <typename Policy>
struct is_fuzzy_unique_policy<Policy, std::void_t<typename Policy::is_fuzzy_policy>> 
    : std::true_type {};

template <typename Policy>
inline constexpr bool is_fuzzy_unique_policy_v = is_fuzzy_unique_policy<Policy>::value;

// =============================================================================
// Logging Policies (Policy-based logging with FatP default)
// =============================================================================

/**
 * @brief No-op logging policy for silent operation.
 */
struct NoLoggingPolicy {
    template <typename... Args>
    static void log(Args&&...) noexcept {}
};

/**
 * @brief Logging policy using Fat-P DiagnosticLogger.
 */
struct FatPLoggingPolicy {
    template <typename T>
    static void log(const char* action, const T& value) {
        std::ostringstream oss;
        oss << "SortedContainer::" << action << ": " << value;
        conditionalPrintError([msg = oss.str()] { return msg; });
    }
    
    static void log(const char* message) {
        conditionalPrintError([message] { return std::string(message); });
    }
};

// =============================================================================
// Uniqueness Policies
// =============================================================================

/**
 * @brief Policy that allows duplicate elements.
 * 
 * Inserts at upper_bound to maintain stable ordering for duplicates,
 * mimicking std::multiset behavior.
 */
struct AllowDuplicatesPolicy {
    /**
     * @brief Inserts the element maintaining sort order, allowing duplicates.
     *
     * @tparam T The element type.
     * @tparam Vector The vector type (usually std::vector<T, Allocator>).
     * @tparam Compare The comparison functor type.
     * @tparam Args Additional arguments (ignored for this policy).
     * @param vec The internal container.
     * @param value The value to insert.
     * @param comp The comparison functor.
     * @return bool Always true (inserted).
     * 
     * @note Complexity: O(log N) search + O(N) insertion.
     */
    template <typename T, typename Vector, typename Compare, typename... Args>
    static bool insert(Vector& vec, T&& value, Compare comp, Args&&...) {
        vec.insert(std::upper_bound(vec.begin(), vec.end(), value, comp), 
                   std::forward<T>(value));
        return true;
    }
};

/**
 * @brief Policy that enforces uniqueness by skipping duplicates.
 * 
 * Uses equivalence check via comparator: !comp(a,b) && !comp(b,a).
 */
struct OnlyUniquePolicy {
    /**
     * @brief Inserts only if no equivalent element exists.
     *
     * @tparam T The element type.
     * @tparam Vector The vector type.
     * @tparam Compare The comparison functor type.
     * @tparam Args Additional arguments (ignored for this policy).
     * @param vec The internal container.
     * @param value The value to insert.
     * @param comp The comparison functor.
     * @return bool True if inserted, false if duplicate skipped.
     * 
     * @note Complexity: O(log N) search + O(N) insertion.
     */
    template <typename T, typename Vector, typename Compare, typename... Args>
    static bool insert(Vector& vec, T&& value, Compare comp, Args&&...) {
        auto it = std::lower_bound(vec.begin(), vec.end(), value, comp);
        if (it == vec.end() || comp(value, *it) || comp(*it, value)) {
            vec.insert(it, std::forward<T>(value));
            return true;
        }
        return false;
    }
};

/**
 * @brief Policy for fuzzy uniqueness using EqualityComparisons policies.
 * 
 * Uses approximate equality (via areEqual) rather than exact comparison.
 * Suitable for floating-point types where exact equality is unreliable.
 * 
 * @tparam EqPolicy The equality policy (e.g., HybridComparisonPolicy).
 * @tparam EpsParams Types for epsilon params (e.g., double for tolerance).
 * 
 * @warning Fuzzy equality is non-transitive. This implementation checks
 * only adjacent elements after binary search, which may miss some fuzzy
 * duplicates in edge cases. For strict fuzzy uniqueness, consider a full
 * linear scan.
 */
template <typename EqPolicy = HybridComparisonPolicy, typename... EpsParams>
struct FuzzyUniquePolicy {
    using is_fuzzy_policy = void;  ///< Trait marker for detection
    using EqPolicy_t = EqPolicy;   ///< Exposed equality policy type
    
    /**
     * @brief Inserts only if no approximately equal element exists.
     * 
     * @param vec The internal container.
     * @param value The value to insert.
     * @param comp The comparison functor.
     * @param eps Epsilon params for areEqual.
     * @return bool True if inserted, false if fuzzy duplicate skipped.
     * 
     * @note Complexity: O(log N) search + O(N) insertion.
     */
    template <typename T, typename Vector, typename Compare, typename... EpsArgs>
    static bool insert(Vector& vec, T&& value, Compare comp, EpsArgs... eps) {
        auto it = std::lower_bound(vec.begin(), vec.end(), value, comp);
        
        // Check for fuzzy duplicate at insertion point
        if (it != vec.end() && areEqual<T, EqPolicy>(*it, value, eps...)) {
            return false;
        }
        
        // Check for fuzzy duplicate before insertion point
        if (it != vec.begin()) {
            auto prev = std::prev(it);
            if (areEqual<T, EqPolicy>(*prev, value, eps...)) {
                return false;
            }
        }
        
        // FIX: Eliminate redundant binary search - use upper_bound starting from 'it'
        auto insert_pos = std::upper_bound(it, vec.end(), value, comp);
        vec.insert(insert_pos, std::forward<T>(value));
        return true;
    }
};

// =============================================================================
// Default Epsilon Helpers
// =============================================================================

/**
 * @brief Helper to get default epsilon for a type.
 * 
 * Uses values from ComparisonTolerances.h for float/double.
 * For non-floating-point types, returns a default-constructed T.
 * 
 * @note Uses a function-based approach to avoid constexpr issues with
 * non-literal types like user-defined classes.
 */
template <typename T, typename = void>
struct default_epsilon_impl {
    static T get() { return T{}; }
};

template <>
struct default_epsilon_impl<float, void> {
    static constexpr float get() noexcept { return kDefaultFloatEpsilon; }
};

template <>
struct default_epsilon_impl<double, void> {
    static constexpr double get() noexcept { return kDefaultDoubleEpsilon; }
};

/**
 * @brief Get default epsilon value for type T.
 * 
 * For float/double, returns the standard tolerance from ComparisonTolerances.h.
 * For other types, returns T{} (useful for integer types where epsilon = 0).
 */
template <typename T>
inline auto default_epsilon_v() -> decltype(default_epsilon_impl<T>::get()) {
    return default_epsilon_impl<T>::get();
}

// =============================================================================
// Logging Unique Policy Wrapper (FIX: Variadic forwarding)
// =============================================================================

/**
 * @brief Logging policy wrapper that logs inserts via configurable logger.
 * 
 * Wraps any uniqueness policy and logs insert operations.
 * 
 * @tparam BasePolicy The base uniqueness policy to wrap.
 * @tparam Logger The logging policy (defaults to FatPLoggingPolicy).
 */
template <typename BasePolicy,
          typename Logger = FatPLoggingPolicy,
          bool IsFuzzy = is_fuzzy_unique_policy_v<BasePolicy>>
struct LoggingUniquePolicy;

// Non-fuzzy base policy specialization
template <typename BasePolicy, typename Logger>
struct LoggingUniquePolicy<BasePolicy, Logger, false> : BasePolicy {
    /**
     * @brief Logs and forwards insert to base policy.
     *
     * Uses variadic forwarding to preserve move semantics and pass through any
     * extra policy arguments (e.g., epsilon parameters for fuzzy policies).
     */
    template <typename T, typename Vector, typename Compare, typename... Args>
    static bool insert(Vector& vec, T&& value, Compare comp, Args&&... args) {
        Logger::log("insert", value);
        return BasePolicy::insert(vec, std::forward<T>(value), comp,
                                  std::forward<Args>(args)...);
    }
};

// Fuzzy base policy specialization - propagates fuzzy marker and EqPolicy_t
template <typename BasePolicy, typename Logger>
struct LoggingUniquePolicy<BasePolicy, Logger, true> : BasePolicy {
    using is_fuzzy_policy = void;               ///< Trait marker for detection
    using EqPolicy_t = typename BasePolicy::EqPolicy_t;  ///< Propagate equality policy

    /**
     * @brief Logs and forwards insert to base policy.
     *
     * Uses variadic forwarding to preserve move semantics and pass through any
     * extra policy arguments (e.g., epsilon parameters).
     */
    template <typename T, typename Vector, typename Compare, typename... Args>
    static bool insert(Vector& vec, T&& value, Compare comp, Args&&... args) {
        Logger::log("insert", value);
        return BasePolicy::insert(vec, std::forward<T>(value), comp,
                                  std::forward<Args>(args)...);
    }
};

// =============================================================================
// SortedContainer Class
// =============================================================================

/**
 * @brief A container that maintains its elements in sorted order at all times.
 *
 * Uses Policy-Based Design for uniqueness, comparison, allocation, and
 * concurrency logic. It is designed to be a thread-safe replacement for
 * a sorted `std::vector` when configured with `MutexSynchronizationPolicy` 
 * or `RWLockSynchronizationPolicy`.
 *
 * @tparam T The element type (must be comparable via ComparePolicy; movable for efficiency).
 * @tparam UniquenessPolicy Controls handling of duplicates
 *         (AllowDuplicatesPolicy, OnlyUniquePolicy, FuzzyUniquePolicy, etc.).
 * @tparam ComparePolicy The comparison function/functor (defaults to std::less<T>).
 * @tparam Allocator The allocator type used for memory management
 *         (defaults to std::allocator<T>).
 * @tparam ConcurrencyPolicy The policy governing thread-safety (defaults to
 *         SingleThreadedPolicy, can be MutexSynchronizationPolicy, 
 *         RWLockSynchronizationPolicy, or SpinlockSynchronizationPolicy).
 * @tparam BackendPolicy The backend container policy (defaults to VectorBackendPolicy).
 * 
 * @see AllowDuplicatesPolicy, OnlyUniquePolicy, FuzzyUniquePolicy
 * @see SingleThreadedPolicy, MutexSynchronizationPolicy
 */
template <
    typename T,
    typename UniquenessPolicy = AllowDuplicatesPolicy,
    typename ComparePolicy = std::less<T>,
    typename Allocator = std::allocator<T>,
    typename ConcurrencyPolicy = SingleThreadedPolicy,
    template <typename, typename> class BackendPolicy = VectorBackendPolicy
>
class SortedContainer : public ConcurrencyPolicy {
public:
    // =========================================================================
    // Type Aliases (snake_case for STL consistency)
    // =========================================================================
    
    using value_type = T;
    using allocator_type = Allocator;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = typename std::allocator_traits<Allocator>::pointer;
    using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

private:
    using InternalContainer = typename BackendPolicy<T, Allocator>::type;
    
    // FIX: Member naming convention (mPascalCase)
    EnforcedInit<InternalContainer> mInternalContainer;
    ComparePolicy mCompare{};

public:
    using iterator = typename InternalContainer::iterator;
    using const_iterator = typename InternalContainer::const_iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // =========================================================================
    // Constructors
    // =========================================================================
    
    /**
     * @brief Default constructor.
     * @throws ContractViolation if container initialization fails.
     */
    SortedContainer() {
        auto init_res = mInternalContainer.init();
        FATP_ALWAYS_ENFORCE(static_cast<bool>(init_res), "Failed to initialize container");
    }
    
    /**
     * @brief Range constructor: inserts elements from an iterator range.
     *
     * @tparam InputIt Iterator type.
     * @tparam EpsParams Epsilon parameter types for fuzzy policies.
     * @param first Start of the range.
     * @param last End of the range.
     * @param eps Optional epsilon params for fuzzy policies.
     * @throws ContractViolation if initialization or insertion fails.
     * 
     * @note Complexity: O(N log N) where N = distance(first, last).
     */
    template <typename InputIt, typename... EpsParams>
    SortedContainer(InputIt first, InputIt last, EpsParams... eps) {
        auto init_res = mInternalContainer.init();
        FATP_ALWAYS_ENFORCE(static_cast<bool>(init_res), "Failed to initialize container");
        auto insert_res = insertRange(first, last, eps...);
        if (!insert_res.has_value()) {
            FATP_ALWAYS_ENFORCE(false, "Failed to insert range: " + insert_res.error());
        }
    }
    
    /**
     * @brief Initializer list constructor.
     * @param init Initializer list of elements.
     * @param eps Optional epsilon params for fuzzy policies.
     */
    template <typename... EpsParams>
    SortedContainer(std::initializer_list<T> init, EpsParams... eps)
        : SortedContainer(init.begin(), init.end(), eps...) {}

    // =========================================================================
    // Iterators
    // =========================================================================
    
    /**
     * @brief Returns a const iterator to the beginning.
     * @return const_iterator
     * 
     * @warning Thread Safety: The returned iterator is only valid while an
     * external lock is held. For thread-safe iteration, use forEach() or
     * withInternalContainer().
     */
    [[nodiscard]] const_iterator begin() const {
        auto guard = this->lock_shared();
        return mInternalContainer.get().begin();
    }
    
    /**
     * @brief Returns a const iterator to the end.
     * @return const_iterator
     * 
     * @warning Thread Safety: See begin().
     */
    [[nodiscard]] const_iterator end() const {
        auto guard = this->lock_shared();
        return mInternalContainer.get().end();
    }
    
    /**
     * @brief Returns a const iterator to the beginning (explicit const version).
     * @return const_iterator
     */
    [[nodiscard]] const_iterator cbegin() const {
        return begin();
    }
    
    /**
     * @brief Returns a const iterator to the end (explicit const version).
     * @return const_iterator
     */
    [[nodiscard]] const_iterator cend() const {
        return end();
    }
    
    /**
     * @brief Returns a reverse const iterator to the reverse beginning.
     * @return const_reverse_iterator
     * 
     * @warning Thread Safety: See begin().
     */
    [[nodiscard]] const_reverse_iterator rbegin() const {
        auto guard = this->lock_shared();
        return mInternalContainer.get().rbegin();
    }
    
    /**
     * @brief Returns a reverse const iterator to the reverse end.
     * @return const_reverse_iterator
     * 
     * @warning Thread Safety: See begin().
     */
    [[nodiscard]] const_reverse_iterator rend() const {
        auto guard = this->lock_shared();
        return mInternalContainer.get().rend();
    }
    
    /**
     * @brief Returns a const reverse iterator to the reverse beginning.
     * @return const_reverse_iterator
     */
    [[nodiscard]] const_reverse_iterator crbegin() const {
        return rbegin();
    }
    
    /**
     * @brief Returns a const reverse iterator to the reverse end.
     * @return const_reverse_iterator
     */
    [[nodiscard]] const_reverse_iterator crend() const {
        return rend();
    }

    // =========================================================================
    // Thread-Safe Iteration
    // =========================================================================
    
    /**
     * @brief Thread-safe iteration with callback.
     * 
     * Holds the read lock for the entire iteration, ensuring element validity.
     * This is the recommended way to iterate in multi-threaded code.
     *
     * @tparam Func Callable type accepting const T&.
     * @param func The function to execute for each element.
     * 
     * @note Complexity: O(N).
     * 
     * @code
     * container.forEach([](const auto& elem) {
     *     std::cout << elem << '\n';
     * });
     * @endcode
     */
    template <typename Func>
    void forEach(Func&& func) const {
        auto guard = this->lock_shared();
        for (const auto& elem : mInternalContainer.get()) {
            std::forward<Func>(func)(elem);
        }
    }
    
    /**
     * @brief Thread-safe iteration with early exit.
     * 
     * @tparam Func Callable type accepting const T& and returning bool.
     * @param func The function to execute. Return false to stop iteration.
     * @return bool True if iteration completed, false if stopped early.
     */
    template <typename Func>
    bool forEachWhile(Func&& func) const {
        auto guard = this->lock_shared();
        for (const auto& elem : mInternalContainer.get()) {
            if (!std::forward<Func>(func)(elem)) {
                return false;
            }
        }
        return true;
    }

    // =========================================================================
    // Capacity
    // =========================================================================
    
    /**
     * @brief Returns the number of elements.
     * @return size_type
     * @note Complexity: O(1).
     */
    [[nodiscard]] size_type size() const {
        auto guard = this->lock_shared();
        return mInternalContainer.get().size();
    }
    
    /**
     * @brief Checks if the container is empty.
     * @return bool True if empty.
     * @note Complexity: O(1).
     */
    [[nodiscard]] bool empty() const {
        auto guard = this->lock_shared();
        return mInternalContainer.get().empty();
    }
    
    /**
     * @brief Returns the current capacity.
     * @return size_type The capacity (or size for containers without capacity).
     * @note For containers without capacity() (e.g., deque), returns size().
     * @note Complexity: O(1).
     */
    [[nodiscard]] size_type capacity() const {
        auto guard = this->lock_shared();
        if constexpr (has_reserve_v<InternalContainer>) {
            return mInternalContainer.get().capacity();
        } else {
            return mInternalContainer.get().size();
        }
    }
    
    /**
     * @brief Returns the maximum possible size.
     * @return size_type
     */
    [[nodiscard]] size_type max_size() const {
        auto guard = this->lock_shared();
        return mInternalContainer.get().max_size();
    }
    
    /**
     * @brief Reserves storage for at least n elements.
     * @param n The number of elements to reserve space for.
     * @note No-op for containers without reserve() (e.g., deque).
     * @note Complexity: O(N) if reallocation occurs.
     */
    void reserve(size_type n) {
        auto guard = this->lock();
        if constexpr (has_reserve_v<InternalContainer>) {
            mInternalContainer.get().reserve(n);
        }
    }
    
    /**
     * @brief Reduces capacity to fit size.
     * @note No-op for containers without shrink_to_fit().
     * @note Complexity: O(N) if reallocation occurs.
     */
    void shrink_to_fit() {
        auto guard = this->lock();
        if constexpr (has_shrink_to_fit_v<InternalContainer>) {
            mInternalContainer.get().shrink_to_fit();
        }
    }

    // =========================================================================
    // Modifiers
    // =========================================================================
    
    /**
     * @brief Clears all elements from the container.
     * @note Complexity: O(N).
     */
    void clear() {
        auto guard = this->lock();
        mInternalContainer.get().clear();
        validateInvariant_unlocked();
    }
    
    /**
     * @brief Inserts a value while maintaining sorted order.
     *
     * @tparam U Value type (must be convertible to T).
     * @tparam EpsParams Epsilon parameter types for fuzzy policies.
     * @param value The value to insert.
     * @param eps Optional epsilon params for fuzzy policies.
     * @return Expected<bool, std::string> True if inserted, false if skipped 
     *         (duplicate), or error on failure.
     * 
     * @note Complexity: O(log N) search + O(N) insertion.
     */
    template <typename U = T, typename... EpsParams, 
              typename = std::enable_if_t<std::is_convertible_v<U, T>>>
    [[nodiscard]] Expected<bool, std::string> insert(U&& value, EpsParams... eps) {
        auto guard = this->lock();
        static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>, 
                      "T must be movable or copyable");
        try {
            bool inserted;
            if constexpr (is_fuzzy_unique_policy_v<UniquenessPolicy>) {
                if constexpr (sizeof...(eps) == 0) {
                    inserted = UniquenessPolicy::insert(
                        mInternalContainer.get(), std::forward<U>(value), mCompare,
                        default_epsilon_v<T>(), default_epsilon_v<T>());
                } else {
                    inserted = UniquenessPolicy::insert(
                        mInternalContainer.get(), std::forward<U>(value), mCompare, eps...);
                }
            } else {
                inserted = UniquenessPolicy::insert(
                    mInternalContainer.get(), std::forward<U>(value), mCompare);
            }
            validateInvariant_unlocked();
            return Expected<bool, std::string>(inserted);
        } catch (const std::exception& e) {
            return unexpected<std::string>(e.what());
        }
    }
    
    /**
     * @brief Constructs element in-place.
     * 
     * @tparam Args Constructor argument types.
     * @param args Arguments forwarded to T's constructor.
     * @return Expected<bool, std::string> True if inserted.
     * 
     * @note Complexity: O(log N) search + O(N) insertion.
     */
    template <typename... Args>
    [[nodiscard]] Expected<bool, std::string> emplace(Args&&... args) {
        return insert(T(std::forward<Args>(args)...));
    }
    
    /**
     * @brief Batch insert: Appends elements and sorts once.
     *
     * @tparam InputIt Iterator type.
     * @tparam EpsParams Epsilon parameter types.
     * @param first Start of the range.
     * @param last End of the range.
     * @param eps Optional epsilon params for fuzzy uniqueness.
     * @return Expected<void, std::string> Success or error.
     * 
     * @note Complexity: O((M + N) log (M + N)) where M = current size, 
     *       N = range size. For small ranges (<16), uses optimized insertion sort.
     */
    template <typename InputIt, typename... EpsParams>
    [[nodiscard]] Expected<void, std::string> insertRange(InputIt first, InputIt last, 
                                                           EpsParams... eps) {
        auto guard = this->lock();
        try {
            using iter_category = typename std::iterator_traits<InputIt>::iterator_category;
            
            if constexpr (std::is_base_of_v<std::forward_iterator_tag, iter_category>) {
                auto range_size = std::distance(first, last);
                if (range_size <= 0) {
                    return {};
                }
                
                auto current_size = mInternalContainer.get().size();
                auto new_size = checked_add<ThrowOnErrorPolicy>(
                    current_size, static_cast<size_t>(range_size));
                    
                if constexpr (has_reserve_v<InternalContainer>) {
                    mInternalContainer.get().reserve(new_size);
                }
                mInternalContainer.get().insert(mInternalContainer.get().end(), first, last);
                
                if (mInternalContainer.get().size() != new_size) {
                    return unexpected<std::string>("Container size mismatch after insertion");
                }
                
                // Optimized sort: insertion sort for small, stable_sort for large
                if (range_size < 16) {
                    auto container_end = mInternalContainer.get().end();
                    auto start_it = container_end - range_size;
                    
                    if (start_it < mInternalContainer.get().begin() || 
                        start_it > container_end) {
                        return unexpected<std::string>(
                            "Invalid iterator range in insertion sort");
                    }
                    
                    for (auto it = start_it; it != container_end; ++it) {
                        auto insertion_point = std::upper_bound(
                            mInternalContainer.get().begin(), it, *it, mCompare);
                        std::rotate(insertion_point, it, it + 1);
                    }
                } else {
                    std::stable_sort(mInternalContainer.get().begin(), 
                                     mInternalContainer.get().end(), mCompare);
                }
            } else {
                // Input iterators: single-pass, cannot call distance without consuming
                // Just append and sort -- no optimization possible
                mInternalContainer.get().insert(mInternalContainer.get().end(), first, last);
                std::stable_sort(mInternalContainer.get().begin(), 
                                 mInternalContainer.get().end(), mCompare);
            }
            
            // Apply uniqueness logic
            applyUniquenessPolicy_unlocked(eps...);
            
            validateInvariant_unlocked();
            return {};
        } catch (const std::exception& e) {
            return unexpected<std::string>(e.what());
        }
    }
    
    /**
     * @brief Erases the first occurrence of value.
     * 
     * @param value The value to erase.
     * @return Expected<bool, std::string> True if erased, false if not found.
     * 
     * @note Complexity: O(log N) find + O(N) erase.
     */
    [[nodiscard]] Expected<bool, std::string> erase(const T& value) {
        auto guard = this->lock();
        try {
            auto it = find_unlocked(value);
            if (it == mInternalContainer.get().end()) {
                return Expected<bool, std::string>(false);
            }
            mInternalContainer.get().erase(it);
            validateInvariant_unlocked();
            return Expected<bool, std::string>(true);
        } catch (const std::exception& e) {
            return unexpected<std::string>(e.what());
        }
    }
    
    /**
     * @brief Erases all elements matching predicate.
     * 
     * @tparam Pred Unary predicate type.
     * @param pred Predicate returning true for elements to erase.
     * @return size_type Number of elements erased.
     * 
     * @note Complexity: O(N).
     */
    template <typename Pred>
    size_type erase_if(Pred pred) {
        auto guard = this->lock();
        auto& c = mInternalContainer.get();
        auto old_size = c.size();
        c.erase(std::remove_if(c.begin(), c.end(), pred), c.end());
        validateInvariant_unlocked();
        return old_size - c.size();
    }
    
    /**
     * @brief Erases elements in value range [first_val, last_val).
     * 
     * @param first_val Lower bound (inclusive).
     * @param last_val Upper bound (exclusive).
     * @return size_type Number of elements erased.
     * 
     * @note Complexity: O(log N) search + O(K) erase where K = erased count.
     */
    size_type erase_range(const T& first_val, const T& last_val) {
        auto guard = this->lock();
        auto& c = mInternalContainer.get();
        auto first = std::lower_bound(c.begin(), c.end(), first_val, mCompare);
        auto last = std::lower_bound(first, c.end(), last_val, mCompare);
        auto count = static_cast<size_type>(std::distance(first, last));
        c.erase(first, last);
        validateInvariant_unlocked();
        return count;
    }

    // =========================================================================
    // Lookup
    // =========================================================================
    
    /**
     * @brief Counts occurrences of value.
     * 
     * @param value The value to count.
     * @return size_type Number of matches (0 or 1 for unique policies).
     * 
     * @note Complexity: O(log N + K) where K = matches.
     */
    [[nodiscard]] size_type count(const T& value) const {
        auto guard = this->lock_shared();
        auto [lower, upper] = std::equal_range(
            mInternalContainer.get().begin(), mInternalContainer.get().end(), 
            value, mCompare);
        return static_cast<size_type>(std::distance(lower, upper));
    }
    
    /**
     * @brief Checks if element exists.
     * 
     * More efficient than find() != end() for existence checks.
     * 
     * @param value The value to check.
     * @return bool True if found.
     * 
     * @note Complexity: O(log N).
     */
    [[nodiscard]] bool contains(const T& value) const {
        auto guard = this->lock_shared();
        auto it = std::lower_bound(
            mInternalContainer.get().begin(), 
            mInternalContainer.get().end(), 
            value, mCompare);
        return it != mInternalContainer.get().end() && 
               !mCompare(*it, value) && !mCompare(value, *it);
    }
    
    /**
     * @brief Finds an element using binary search.
     *
     * @param value The value to find.
     * @return const_iterator Iterator to the element, or end() if not found.
     * 
     * @warning Thread Safety: See begin().
     * @note Complexity: O(log N).
     */
    [[nodiscard]] const_iterator find(const T& value) const {
        auto guard = this->lock_shared();
        return find_unlocked(value);
    }
    
    /**
     * @brief Thread-safe find returning a copy.
     * 
     * Unlike find(), the returned value remains valid after the lock is released.
     * This is the preferred method for thread-safe lookups.
     *
     * @param value The value to find.
     * @return std::optional<T> The found element, or std::nullopt if not found.
     * 
     * @note Complexity: O(log N).
     */
    [[nodiscard]] std::optional<T> findCopy(const T& value) const {
        auto guard = this->lock_shared();
        auto it = find_unlocked(value);
        if (it != mInternalContainer.get().end()) {
            return *it;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Executes a callback on the found element while holding the lock.
     * 
     * This is the safest way to work with found elements in multi-threaded code.
     *
     * @tparam Func Callable type accepting const T&.
     * @param value The value to find.
     * @param func The function to execute if found.
     * @return bool True if the element was found and func was called.
     * 
     * @note Complexity: O(log N).
     */
    template <typename Func>
    bool findApply(const T& value, Func&& func) const {
        auto guard = this->lock_shared();
        auto it = find_unlocked(value);
        if (it != mInternalContainer.get().end()) {
            std::forward<Func>(func)(*it);
            return true;
        }
        return false;
    }
    
    /**
     * @brief Returns iterator to first element not less than value.
     * 
     * @param value The value to search for.
     * @return const_iterator
     * 
     * @warning Thread Safety: See begin().
     * @note Complexity: O(log N).
     */
    [[nodiscard]] const_iterator lower_bound(const T& value) const {
        auto guard = this->lock_shared();
        return std::lower_bound(mInternalContainer.get().begin(), 
                                mInternalContainer.get().end(), value, mCompare);
    }
    
    /**
     * @brief Returns iterator to first element greater than value.
     * 
     * @param value The value to search for.
     * @return const_iterator
     * 
     * @warning Thread Safety: See begin().
     * @note Complexity: O(log N).
     */
    [[nodiscard]] const_iterator upper_bound(const T& value) const {
        auto guard = this->lock_shared();
        return std::upper_bound(mInternalContainer.get().begin(), 
                                mInternalContainer.get().end(), value, mCompare);
    }
    
    /**
     * @brief Returns range of elements equivalent to value.
     * 
     * @param value The value to search for.
     * @return std::pair<const_iterator, const_iterator> Range [lower, upper).
     * 
     * @warning Thread Safety: See begin().
     * @note Complexity: O(log N).
     */
    [[nodiscard]] std::pair<const_iterator, const_iterator> 
    equal_range(const T& value) const {
        auto guard = this->lock_shared();
        return std::equal_range(mInternalContainer.get().begin(),
                                mInternalContainer.get().end(), value, mCompare);
    }

    // =========================================================================
    // Observers
    // =========================================================================
    
    /**
     * @brief Returns the comparison functor.
     * @return ComparePolicy
     */
    [[nodiscard]] ComparePolicy key_comp() const {
        return mCompare;
    }
    
    /**
     * @brief Returns the comparison functor (alias for key_comp).
     * @return ComparePolicy
     */
    [[nodiscard]] ComparePolicy value_comp() const {
        return mCompare;
    }

    // =========================================================================
    // Interoperability
    // =========================================================================
    
    /**
     * @brief Returns a thread-safe copy of the internal data.
     * @return InternalContainer A copy of the internal container.
     * @note Complexity: O(N).
     */
    [[nodiscard]] InternalContainer toVector() const {
        auto guard = this->lock_shared();
        return mInternalContainer.get();
    }
    
    /**
     * @brief Alias for toVector() - returns a thread-safe snapshot.
     * @return InternalContainer A copy of the internal container.
     */
    [[nodiscard]] InternalContainer snapshot() const {
        return toVector();
    }
    
    /**
     * @brief Executes a function with scoped access to the internal container.
     *
     * Holds the read lock during execution, ensuring thread-safety for 
     * read-only operations without exposing the reference.
     * 
     * @tparam Func Callable taking const InternalContainer&.
     * @param func The function to execute.
     * @return The return value of func.
     * 
     * @code
     * auto sum = container.withInternalContainer([](const auto& vec) {
     *     return std::accumulate(vec.begin(), vec.end(), 0);
     * });
     * @endcode
     */
    template <typename Func>
    decltype(auto) withInternalContainer(Func&& func) const {
        auto guard = this->lock_shared();
        return std::forward<Func>(func)(mInternalContainer.get());
    }
    
    /**
     * @brief Manually validates the sorted invariant.
     * 
     * @throws ContractViolation if the invariant is violated (debug only).
     * @note This check is disabled in Release builds (NDEBUG defined).
     */
    void validateInvariant() const {
        auto guard = this->lock_shared();
        validateInvariant_unlocked();
    }

private:
    // =========================================================================
    // Private Helpers (caller must hold lock)
    // =========================================================================
    
    /**
     * @brief Non-locking find for internal use.
     * @warning Caller MUST hold appropriate lock before calling.
     */
    const_iterator find_unlocked(const T& value) const {
        auto it = std::lower_bound(
            mInternalContainer.get().begin(), 
            mInternalContainer.get().end(), 
            value, mCompare);
        if (it != mInternalContainer.get().end() && 
            !mCompare(*it, value) && !mCompare(value, *it)) {
            return it;
        }
        return mInternalContainer.get().end();
    }
    
    /**
     * @brief Non-locking invariant check for internal use.
     * @warning Caller MUST hold appropriate lock before calling.
     */
    void validateInvariant_unlocked() const {
        FATP_ENFORCE(std::is_sorted(
            mInternalContainer.get().begin(), 
            mInternalContainer.get().end(), 
            mCompare),
            "SortedContainer invariant violated: container is not sorted."
        );
    }
    
    /**
     * @brief Applies uniqueness policy after batch insert.
     * @warning Caller MUST hold lock.
     */
    template <typename... EpsParams>
    void applyUniquenessPolicy_unlocked(EpsParams... eps) {
        if constexpr (std::is_base_of_v<OnlyUniquePolicy, UniquenessPolicy>) {
            const auto& comp_ref = mCompare;
            auto equiv = [&comp_ref](const T& a, const T& b) {
                return !comp_ref(a, b) && !comp_ref(b, a);
            };
            auto lastUnique = std::unique(mInternalContainer.get().begin(), 
                                           mInternalContainer.get().end(), equiv);
            mInternalContainer.get().erase(lastUnique, mInternalContainer.get().end());
        }
        else if constexpr (is_fuzzy_unique_policy_v<UniquenessPolicy>) {
            auto& container = mInternalContainer.get();
            if (!container.empty()) {
                auto eps_tuple = [&]() {
                    if constexpr (sizeof...(eps) == 0) {
                        return std::make_tuple(default_epsilon_v<T>(), default_epsilon_v<T>());
                    } else {
                        return std::make_tuple(eps...);
                    }
                }();
                
                auto fuzzy_equal = [this, &eps_tuple](const T& lhs, const T& rhs) {
                    return std::apply([this, &lhs, &rhs](auto&&... args) {
                        return areEqual<T, typename UniquenessPolicy::EqPolicy_t>(
                            lhs, rhs, args...);
                    }, eps_tuple);
                };
                
                auto write_it = container.begin();
                for (auto read_it = container.begin() + 1; 
                     read_it != container.end(); ++read_it) {
                    if (!fuzzy_equal(*write_it, *read_it)) {
                        ++write_it;
                        if (write_it != read_it) {
                            *write_it = std::move(*read_it);
                        }
                    }
                }
                container.erase(write_it + 1, container.end());
            }
        }
    }
};

// =============================================================================
// Type Trait Specialization
// =============================================================================

template <typename T, typename UP, typename CP, typename A, typename ConP, 
          template <typename, typename> class BP>
struct is_sorted_container<SortedContainer<T, UP, CP, A, ConP, BP>> : std::true_type {};

// =============================================================================
// Comparison Operators
// =============================================================================

/**
 * @brief Equality comparison.
 * @note Thread-safe: acquires locks on both containers.
 */
template <typename T, typename UP, typename CP, typename A, typename ConP, 
          template <typename, typename> class BP>
bool operator==(
    const SortedContainer<T, UP, CP, A, ConP, BP>& lhs,
    const SortedContainer<T, UP, CP, A, ConP, BP>& rhs) 
{
    return lhs.toVector() == rhs.toVector();
}

/**
 * @brief Inequality comparison.
 */
template <typename T, typename UP, typename CP, typename A, typename ConP,
          template <typename, typename> class BP>
bool operator!=(
    const SortedContainer<T, UP, CP, A, ConP, BP>& lhs,
    const SortedContainer<T, UP, CP, A, ConP, BP>& rhs)
{
    return !(lhs == rhs);
}

/**
 * @brief Less-than comparison (lexicographic).
 */
template <typename T, typename UP, typename CP, typename A, typename ConP,
          template <typename, typename> class BP>
bool operator<(
    const SortedContainer<T, UP, CP, A, ConP, BP>& lhs,
    const SortedContainer<T, UP, CP, A, ConP, BP>& rhs)
{
    return lhs.toVector() < rhs.toVector();
}

/**
 * @brief Less-than-or-equal comparison.
 */
template <typename T, typename UP, typename CP, typename A, typename ConP,
          template <typename, typename> class BP>
bool operator<=(
    const SortedContainer<T, UP, CP, A, ConP, BP>& lhs,
    const SortedContainer<T, UP, CP, A, ConP, BP>& rhs)
{
    return !(rhs < lhs);
}

/**
 * @brief Greater-than comparison.
 */
template <typename T, typename UP, typename CP, typename A, typename ConP,
          template <typename, typename> class BP>
bool operator>(
    const SortedContainer<T, UP, CP, A, ConP, BP>& lhs,
    const SortedContainer<T, UP, CP, A, ConP, BP>& rhs)
{
    return rhs < lhs;
}

/**
 * @brief Greater-than-or-equal comparison.
 */
template <typename T, typename UP, typename CP, typename A, typename ConP,
          template <typename, typename> class BP>
bool operator>=(
    const SortedContainer<T, UP, CP, A, ConP, BP>& lhs,
    const SortedContainer<T, UP, CP, A, ConP, BP>& rhs)
{
    return !(lhs < rhs);
}

} // namespace fat_p
