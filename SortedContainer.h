/**
 * @file SortedContainer.h
 * @brief A policy-based sorted vector container that maintains order on inserts.
 *
 * @details Uses Policy-Based Design for uniqueness, comparison, allocation, and concurrency.
 * Supports batch inserts with sorting. Read-only iterators to preserve order.
 * Integrates with library's DbC (enforce.h), Expected for error handling, and EqualityComparisons for fuzzy compares.
 * Conditional thread-safety via ConcurrencyPolicy (SingleThreaded, Mutex, RWLock, or Spinlock).
 * Extensible with new policies (e.g., LoggingPolicy for inserts, TransformUniquenessPolicy).
 * Wish-List: Erase support, lower/upper_bound/count/reverse_iterator, fuzzy uniqueness, auto-invariant checks in debug,
 * const T inserts, movable-only T support, EXPECTED_TRY in methods.
 * Optimized: Reserve aggressively; insertion sort for small batches (<16); SIMD note for future numeric inserts.
 * C++17 compliant; header-only; no external deps (guards optional <shared_mutex>/<atomic>).
 *
 * @note Invariant checks are debug-only (via enforce); release has zero overhead.
 * @note Supports movable-only T via move inserts; tests with floats for fuzzy.
 * @note Exposes internal vector with warnings (Modifying may break invariant).
 */
#pragma once
#if !defined(CPP_UTILITIES_USE_SHARED_MUTEX)
#define CPP_UTILITIES_USE_SHARED_MUTEX 1 // Enable by default; undef to disable
#endif
#if !defined(CPP_UTILITIES_USE_ATOMIC)
#define CPP_UTILITIES_USE_ATOMIC 1 // Enable by default
#endif
#if !defined(CPP_UTILITIES_USE_IMMINTRIN)
#define CPP_UTILITIES_USE_IMMINTRIN 0 // Disable by default; enable for SIMD
#endif
#if CPP_UTILITIES_USE_IMMINTRIN
#include <immintrin.h> // For SIMD intrinsics
#endif
#include <algorithm>
#include <vector>
#include <stdexcept>
#include <functional>
#include <type_traits>
#include <mutex> // For Mutex SynchronizationPolicy

#if CPP_UTILITIES_USE_SHARED_MUTEX
#include <shared_mutex> // For RWLock SynchronizationPolicy (C++17; guarded)
#endif
#if CPP_UTILITIES_USE_ATOMIC
#include <atomic> // For Spinlock SynchronizationPolicy (guarded)
#endif
#include <optional> // For optional in some returns if needed

#include "CheckedArithmetic.h" // For safe arithmetic in size/reserve
#include "ConcurrencyPolicies.h" // For synchronization policies (extended with Spinlock/RW)
#include "enforce.h" // For DbC and invariant checks
#include "EnforcedInit.h" // For enforced init of internal vector
#include "EqualityComparisons.h" // For fuzzy equality in uniqueness (optional policy)
#include "DiagnosticLogger.h" // For logging invariant violations
#include "Expected.h" // For non-throwing returns (e.g., insert)
#include "ScopeGuard.h" // For RAII in ops
#include "StrongId.h" // For type-safe indices/sizes
#include "TypeTraits.h"

 // Backend Policies (Extensibility: vector vs deque)
template <typename T, typename Allocator>
struct VectorBackendPolicy {
    using type = std::vector<T, Allocator>;
};
template <typename T, typename Allocator>
struct DequeBackendPolicy {
    using type = std::deque<T, Allocator>;
};
namespace cpp_utilities {
    // --- 1. Uniqueness Policies - Extended ---
    /**
     * @brief Policy that allows duplicate elements.
     */
    struct AllowDuplicatesPolicy {
        /**
         * @brief Inserts the element at the position where the sort order is
         * maintained, allowing duplicates.
         *
         * @tparam T The element type.
         * @tparam Vector The vector type (usually std::vector<T, Allocator>).
         * @tparam Compare The comparison functor type.
         * @param vec The internal container.
         * @param value The value to insert.
         * @param comp The comparison functor.
         * @return bool Always true (inserted).
         */
        template <typename T, typename Vector, typename Compare>
        static bool insert(Vector& vec, T&& value, Compare comp) {
            // Inserts the element at the position of the first element not
            // considered less than value (maintains stability).
            vec.insert(std::lower_bound(vec.begin(), vec.end(), value, comp), std::forward<T>(value));
            return true;
        }
    };
    /**
     * @brief Policy that enforces uniqueness of elements by skipping duplicates.
     */
    struct OnlyUniquePolicy {
        /**
         * @brief Inserts the element only if an equivalent element does not
         * already exist in the container.
         *
         * @tparam T The element type.
         * @tparam Vector The vector type.
         * @tparam Compare The comparison functor type.
         * @param vec The internal container.
         * @param value The value to insert.
         * @param comp The comparison functor.
         * @return bool True if inserted, false if duplicate skipped.
         */
        template <typename T, typename Vector, typename Compare>
        static bool insert(Vector& vec, T&& value, Compare comp) {
            auto it = std::lower_bound(vec.begin(), vec.end(), value, comp);
            // If an equivalent element is NOT found at the insertion point,
            // insert the new value. The check is: (*it is less than value) OR
            // (value is less than *it), which means they are not equivalent.
            if (it == vec.end() || comp(value, *it) || comp(*it, value)) {
                vec.insert(it, std::forward<T>(value));
                return true;
            }
            // else: Duplicate found, insertion is skipped.
            return false;
        }
    };
    /**
     * @brief Policy for fuzzy uniqueness using EqualityComparisons policies.
     * @tparam EqPolicy The equality policy (e.g., HybridComparisonPolicy).
     * @tparam EpsParams Types for epsilon params (e.g., double for tol).
     */
    template <typename EqPolicy = HybridComparisonPolicy, typename... EpsParams>
    struct FuzzyUniquePolicy {
        /**
         * @brief Inserts only if no approximately equal element exists.
         * @param vec The internal container.
         * @param value The value to insert.
         * @param comp The comparison functor.
         * @param eps Epsilon params for areEqual.
         * @return bool True if inserted, false if fuzzy duplicate skipped.
         */
        template <typename T, typename Vector, typename Compare>
        static bool insert(Vector& vec, T&& value, Compare comp, EpsParams... eps) {
            auto it = std::lower_bound(vec.begin(), vec.end(), value, comp);
            if (it != vec.end() && areEqual<T, EqPolicy>(*it, value, eps...)) {
                return false; // Fuzzy duplicate, skip
            }
            vec.insert(it, std::forward<T>(value));
            return true;
        }
    };
    /**
     * @brief Logging policy that logs inserts (e.g., via DiagnosticLogger).
     * @tparam BasePolicy The base uniqueness policy to wrap.
     */
    template <typename BasePolicy>
    struct LoggingUniquePolicy : BasePolicy {
        template <typename T, typename Vector, typename Compare>
        static bool insert(Vector& vec, const T& value, Compare comp) {
            conditionalPrintError([] { return "Inserting: " + std::to_string(value); });
            return BasePolicy::insert(vec, value, comp);
        }
    };
    /**
     * @brief TransformUniquenessPolicy: Applies transformer on uniqueness check (view-like for unique).
     * @tparam BasePolicy Base uniqueness.
     * @tparam Transformer Callable to transform T for comparison.
     */
    template <typename BasePolicy, typename Transformer>
    struct TransformUniquenessPolicy : BasePolicy {
        template <typename T, typename Vector, typename Compare>
        static bool insert(Vector& vec, T&& value, Compare comp) {
            Transformer transformer;
            auto transformed_comp = [comp, &transformer](const T& lhs, const T& rhs) {
                return comp(transformer(lhs), transformer(rhs));
                };
            return BasePolicy::insert(vec, std::forward<T>(value), transformed_comp);
        }
    };
    // --- 2. SortedVector Class - Exhaustive Improvements ---
    /**
     * @brief A container that maintains its elements in sorted order at all times.
     *
     * Uses Policy-Based Design for uniqueness, comparison, allocation, and
     * concurrency logic. It is designed to be a thread-safe replacement for
     * a sorted `std::vector` when configured with
     * `MutexSynchronizationPolicy` or `RWLockSynchronizationPolicy`.
     *
     * @tparam T The element type (must be comparable via ComparePolicy; movable for efficiency).
     * @tparam UniquenessPolicy Controls handling of duplicates
     * (AllowDuplicatesPolicy, OnlyUniquePolicy, FuzzyUniquePolicy, etc.).
     * @tparam ComparePolicy The comparison function/functor (defaults to
     * std::less<T>).
     * @tparam Allocator The allocator type used for memory management
     * (defaults to std::allocator<T>).
     * @tparam ConcurrencyPolicy The policy governing thread-safety (defaults to
     * SingleThreadedPolicy, can be set to MutexSynchronizationPolicy, RWLockSynchronizationPolicy, or SpinlockSynchronizationPolicy).
     * @tparam BackendPolicy The backend container policy (defaults to VectorBackendPolicy).
     */
    template <
        typename T,
        typename UniquenessPolicy = AllowDuplicatesPolicy,
        typename ComparePolicy = std::less<T>,
        typename Allocator = std::allocator<T>,
        typename ConcurrencyPolicy = SingleThreadedPolicy,
        template <typename, typename> class BackendPolicy = VectorBackendPolicy
    >
    class SortedVector : public ConcurrencyPolicy {
    private:
        // This type alias must be defined before the public aliases use it.
        using InternalContainer = typename BackendPolicy<T, Allocator>::type;
        // EnforcedInit for internal container (updates: enforced state)
        EnforcedInit<InternalContainer> internalContainer_;
    public:
        // --- Typedefs and Iterators (camelCase for the public API) ---
        using valueType = T;
        using iterator = typename InternalContainer::iterator;
        using constIterator = typename InternalContainer::const_iterator;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<constIterator>;
        // Use StrongId for type-safe size/indices (updates)
        using size_type = StrongId<size_t, struct SizeTag>;
        // --- Constructors ---
        /**
         * @brief Default constructor.
         */
        SortedVector() = default;
        /**
         * @brief Range constructor: inserts elements from an iterator range
         * and sorts the result.
         *
         * @tparam InputIt Iterator type.
         * @param first Start of the range.
         * @param last End of the range.
         */
        template <typename InputIt>
        SortedVector(InputIt first, InputIt last) {
            insertRange(first, last);
        }
        // --- Core STL methods (Read-only access) ---
        /**
         * @brief Returns a const iterator to the beginning of the container.
         * @return constIterator
         */
        [[nodiscard]] constIterator begin() const {
            typename ConcurrencyPolicy::SharedGuard guard(this->getLock()); // Shared for read
            return internalContainer_.get().begin();
        }
        /**
         * @brief Returns a const iterator to the end of the container.
         * @return constIterator
         */
        [[nodiscard]] constIterator end() const {
            typename ConcurrencyPolicy::SharedGuard guard(this->getLock()); // Shared for read
            return internalContainer_.get().end();
        }
        /**
         * @brief Returns a reverse const iterator to the reverse beginning (end).
         * @return const_reverse_iterator
         */
        [[nodiscard]] const_reverse_iterator rbegin() const {
            typename ConcurrencyPolicy::SharedGuard guard(this->getLock());
            return internalContainer_.get().rbegin();
        }
        /**
         * @brief Returns a reverse const iterator to the reverse end (begin).
         * @return const_reverse_iterator
         */
        [[nodiscard]] const_reverse_iterator rend() const {
            typename ConcurrencyPolicy::SharedGuard guard(this->getLock());
            return internalContainer_.get().rend();
        }
        /**
         * @brief Returns the number of elements in the container.
         * @return size_t
         */
        [[nodiscard]] size_type size() const {
            typename ConcurrencyPolicy::SharedGuard guard(this->getLock()); // Shared for read
            return size_type(internalContainer_.get().size());
        }
        /**
         * @brief Checks if the container is empty.
         * @return bool True if empty, false otherwise.
         */
        [[nodiscard]] bool empty() const {
            typename ConcurrencyPolicy::SharedGuard guard(this->getLock()); // Shared for read
            return internalContainer_.get().empty();
        }
        /**
         * @brief Counts occurrences of value (O(log N + K) where K=matches).
         * @param value The value to count.
         * @return size_t Number of matches (1 for unique policies).
         */
        [[nodiscard]] size_type count(const T& value) const {
            typename ConcurrencyPolicy::SharedGuard guard(this->getLock());
            auto [lower, upper] = std::equal_range(internalContainer_.get().begin(), internalContainer_.get().end(), value, compare_);
            return size_type(std::distance(lower, upper));
        }
        // --- Core Functionality (Write Access) ---
        /**
         * @brief Inserts a value while maintaining sorted order, respecting
         * the UniquenessPolicy.
         *
         * @param value The value to insert (forwarded; supports const T&).
         * @param eps Optional epsilon params for fuzzy policies.
         * @return Expected<bool, std::string> True if inserted (or skipped), error on failure (e.g., invariant violation).
         */
        template <typename U = T, typename... EpsParams, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
        Expected<bool, std::string> insert(U&& value, EpsParams... eps) {
            typename ConcurrencyPolicy::LockGuard guard(this->getLock()); // Write lock
            // ScopeGuard for RAII cleanup (updates)
            ScopeGuard<std::function<void()>> cleanup_guard([&]() { validateInvariant(); });
            // Precondition: T movable/copyable if forward
            static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>, "T must be movable or copyable");
            bool inserted;
            if constexpr (std::is_same_v<UniquenessPolicy, FuzzyUniquePolicy<>>) {
                inserted = UniquenessPolicy::insert(internalContainer_.get(), std::forward<U>(value), compare_, eps...);
            }
            else {
                inserted = UniquenessPolicy::insert(internalContainer_.get(), std::forward<U>(value), compare_);
            }
            // Log invariant violation if failed (updates: DiagnosticLogger)
            if (!inserted) {
                conditionalPrintError([] { return "Insert failed: duplicate or error"; });
            }
            return inserted;
        }
        /**
         * @brief Batch insert: Appends elements and sorts once (O(N log N)).
         * For small ranges (<16), uses insertion sort for perf.
         *
         * @tparam InputIt Iterator type.
         * @param first Start of the range.
         * @param last End of the range.
         * @return Expected<void, std::string> Success or error (e.g., invariant).
         */
        template <typename InputIt>
        Expected<void, std::string> insertRange(InputIt first, InputIt last) {
            typename ConcurrencyPolicy::LockGuard guard(this->getLock()); // Write lock
            // ScopeGuard for RAII (updates)
            ScopeGuard<std::function<void()>> cleanup_guard([&]() { validateInvariant(); });
            // Reserve for perf (use CheckedArithmetic for safe add)
            auto range_size = std::distance(first, last);
            auto new_size = checked_add<ThrowOnErrorPolicy>(internalContainer_.get().size(), range_size);
            internalContainer_.get().reserve(new_size);
            internalContainer_.get().insert(internalContainer_.get().end(), first, last);
            // Optimized sort: Insertion sort for small, std::sort for large
            if (range_size < 16) {
                // Insertion sort on appended part (stable, fast for small)
                for (auto it = internalContainer_.get().end() - range_size; it != internalContainer_.get().end(); ++it) {
                    auto insertion_point = std::upper_bound(internalContainer_.get().begin(), it, *it, compare_);
                    std::rotate(insertion_point, it, it + 1);
                }
            }
            else {
                std::sort(internalContainer_.get().begin(), internalContainer_.get().end(), compare_);
            }
            // Apply uniqueness logic if configured for it
            if constexpr (std::is_same_v<UniquenessPolicy, OnlyUniquePolicy>) {
                auto lastUnique = std::unique(internalContainer_.get().begin(), internalContainer_.get().end());
                internalContainer_.get().erase(lastUnique, internalContainer_.get().end());
            }
            else if constexpr (std::is_same_v<UniquenessPolicy, FuzzyUniquePolicy<>>) {
                // Fuzzy unique: Sort first, then remove fuzzy dups (needs custom unique)
                auto fuzzy_unique = [this](const T& lhs, const T& rhs) {
                    return areEqual<T, typename UniquenessPolicy::EqPolicy>(lhs, rhs /*, eps if needed*/);
                    };
                auto lastUnique = std::unique(internalContainer_.get().begin(), internalContainer_.get().end(), fuzzy_unique);
                internalContainer_.get().erase(lastUnique, internalContainer_.get().end());
            }
            return {};
        }
        // --- Erase Support (Wish-List) ---
        /**
         * @brief Erases the first occurrence of value (O(log N) find + O(N) erase).
         * @param value The value to erase.
         * @return Expected<bool, std::string> True if erased, false if not found.
         */
        Expected<bool, std::string> erase(const T& value) {
            typename ConcurrencyPolicy::LockGuard guard(this->getLock()); // Write lock
            // ScopeGuard for RAII (updates)
            ScopeGuard<std::function<void()>> cleanup_guard([&]() { validateInvariant(); });
            auto it = find(value);
            if (it == end()) return false;
            internalContainer_.get().erase(it);
            return true;
        }
        // --- Binary Search Methods (Wish-List: lower/upper_bound) ---
        /**
         * @brief Returns iterator to first element not less than value.
         * @param value The value to search for.
         * @return constIterator
         */
        [[nodiscard]] constIterator lower_bound(const T& value) const {
            typename ConcurrencyPolicy::SharedGuard guard(this->getLock()); // Read lock
            return std::lower_bound(internalContainer_.get().begin(), internalContainer_.get().end(), value, compare_);
        }
        /**
         * @brief Returns iterator to first element greater than value.
         * @param value The value to search for.
         * @return constIterator
         */
        [[nodiscard]] constIterator upper_bound(const T& value) const {
            typename ConcurrencyPolicy::SharedGuard guard(this->getLock()); // Read lock
            return std::upper_bound(internalContainer_.get().begin(), internalContainer_.get().end(), value, compare_);
        }
        // --- Core Functionality (Read Access) ---
        /**
         * @brief Finds an element using binary search (O(log N)).
         *
         * @param value The value to find.
         * @return constIterator An iterator to the element, or end() if not
         * found.
         */
        [[nodiscard]] constIterator find(const T& value) const {
            typename ConcurrencyPolicy::SharedGuard guard(this->getLock()); // Read lock
            auto it = std::lower_bound(internalContainer_.get().begin(), internalContainer_.get().end(), value, compare_);
            // Final check to ensure the found element is actually equivalent to
            // the target using the comparison policy (not just 'not less than').
            if (it != internalContainer_.get().end() && !compare_(*it, value) && !compare_(value, *it)) {
                return it;
            }
            return internalContainer_.get().end();
        }
        /**
         * @brief Manually checks the sorted invariant. Throws a contract
         * violation error if the invariant is violated (DebugOnly).
         *
         * @note This check is disabled in Release builds (NDEBUG is defined)
         * to ensure zero runtime overhead, as the container's methods are
         * designed to guarantee the invariant internally.
         */
        void validateInvariant() const {
            typename ConcurrencyPolicy::SharedGuard guard(this->getLock()); // Read lock
            // Refactored to use the generic 'enforce' macro, which automatically
            // maps to DebugOnlyPolicy, compiling away to nothing in Release builds.
            enforce(std::is_sorted(internalContainer_.get().begin(), internalContainer_.get().end(), compare_),
                "SortedVector invariant violated: container is not sorted."
            );
            // Log violation if failed (updates: DiagnosticLogger)
            if (!std::is_sorted(internalContainer_.get().begin(), internalContainer_.get().end(), compare_)) {
                conditionalPrintError([] { return "Invariant violation logged"; });
            }
        }
        // --- Vector Interoperability Additions ---
        /**
         * @brief Returns a copy of the internal data vector.
         *
         * @return std::vector<T, Allocator> A copy of the internal data.
         */
        [[nodiscard]] InternalContainer toVector() const {
            typename ConcurrencyPolicy::SharedGuard guard(this->getLock()); // Read lock
            return internalContainer_.get();
        }
        /**
         * @brief Returns a const reference to the internal vector.
         *
         * @warning Use with caution: Modifying the returned vector may break the sorted invariant.
         * Modifying Modifying may lead to UB or invariant violations. Prefer toVector() for safe copies or read-only operations.
         * @return const InternalContainer& A read-only reference to the internal data.
         */
        const InternalContainer& asVector() const {
            typename ConcurrencyPolicy::SharedGuard guard(this->getLock()); // Read lock
            return internalContainer_.get();
        }
    private:
        // Member variables are kept at the bottom of the private section.
        ComparePolicy compare_{};
    };

    // --- 3. Additional SynchronizationPolicies (Wish-List: Spinlock) ---
    /**
     * @brief Spinlock policy using std::atomic (busy-wait for low-contention).
     */
    struct SpinlockSynchronizationPolicy {
        mutable std::atomic<bool> lock_{ false };
        class LockGuard {
        public:
            explicit LockGuard(std::atomic<bool>& lock) : lock_(lock) {
                while (lock_.exchange(true, std::memory_order_acquire)) {} // Spin
            }
            ~LockGuard() { lock_.store(false, std::memory_order_release); }
        private:
            std::atomic<bool>& lock_;
        };
        // No shared lock for spin (exclusive only)
        using SharedGuard = LockGuard;
        std::atomic<bool>& getLock() const { return lock_; }
    };
    // Guard optional deps
    // Users define CPP_UTILITIES_USE_SHARED_MUTEX / CPP_UTILITIES_USE_ATOMIC to 0 if not wanted.

} // namespace cpp_utilities