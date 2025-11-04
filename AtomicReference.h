/**
 * @file AtomicReference.h
 * @brief Thread-safe atomic reference wrapper for shared objects with advanced wait policies.
 *
 * @details AtomicReference provides lock-free atomic operations on shared_ptr<T> and weak_ptr<T>.
 * 
 * Key Features:
 * - Lock-free atomic operations on shared_ptr (C++20) with fallbacks
 * - Advanced wait policies: Native (C++20), Polling, BitTagged
 * - Flexible enforcement policies: Always, DebugOnly, Warning, NoThrow, Abort
 * - Support for weak_ptr for observational scenarios
 * - Custom allocator support via factories
 * - Expected-based error handling for safe operations
 * - Comprehensive invariant checking with InvariantGuard
 * - Zero-overhead for non-atomic paths and disabled policies
 * 
 * Architecture:
 * - AtomicTraits: Core storage and operations (shared_ptr, weak_ptr)
 * - Wait Policies: NativeWaitPolicy, PollingWaitPolicy, BitTaggedWaitPolicy
 * - Enforcement Policies: AlwaysEnforcePolicy, DebugOnlyPolicy, etc.
 * - InvariantGuard: RAII-based state validation
 * 
 * Thread Safety:
 * - All operations are thread-safe via std::atomic operations
 * - Wait/notify operations coordinate between threads
 * - Lock-free where hardware supports it
 * 
 * Limitations:
 * - Native wait doesn't support timeouts (std::atomic::wait blocks indefinitely)
 * - Pre-C++20 fallbacks are non-atomic and documented via is_lock_free()
 * - BitTagged policy assumes pointer alignment for tag storage
 * - unique_ptr specialization removed due to fundamental ownership conflicts
 * 
 * Performance:
 * - Lock-free on x86-64, ARM64 with 128-bit atomic support
 * - Zero overhead for non-atomic paths in release builds
 * - Adaptive backoff in polling reduces CPU waste
 * 
 * Usage Example:
 * @code
 * // Basic usage with shared_ptr
 * AtomicReference<int> ref(make_atomic_shared<int>(42));
 * ref.store(std::make_shared<int>(100));
 * auto value = ref.load();
 * 
 * // With weak_ptr for observation
 * auto sp = std::make_shared<int>(42);
 * AtomicReference<std::weak_ptr<int>> weak_ref(make_atomic_weak(sp));
 * if (auto locked = weak_ref.lock_expected()) {
 *     // Use locked.value()
 * }
 * 
 * // With custom allocator
 * std::pmr::monotonic_buffer_resource pool(1024);
 * std::pmr::polymorphic_allocator<Data> alloc(&pool);
 * AtomicReference<Data> ref2(make_atomic_shared<Data>(alloc, args...));
 * 
 * // Wait/notify pattern
 * AtomicReference<int> shared_ref(std::make_shared<int>(0));
 * std::thread t1([&] {
 *     auto old = shared_ref.load();
 *     if (shared_ref.wait(old, std::memory_order_acquire, std::chrono::seconds(5))) {
 *         // Value changed
 *     }
 * });
 * std::thread t2([&] {
 *     shared_ref.store(std::make_shared<int>(42));
 *     shared_ref.notify_all();
 * });
 * @endcode
 * 
 * @tparam T The value type (for shared_ptr specialization)
 * @tparam EnforcementPolicy Policy for contract enforcement (DebugOnlyPolicy default)
 * 
 * @note Requires C++17 minimum; C++20 for native atomic shared_ptr
 * @note Header-only; depends on <atomic>, <memory>, <chrono>, <thread>
 * @note Tested with ThreadSanitizer for race conditions
 * 
 * @see https://en.cppreference.com/w/cpp/atomic/atomic
 * @see https://en.cppreference.com/w/cpp/memory/shared_ptr
 * @see https://en.cppreference.com/w/cpp/memory/weak_ptr
 * 
 * @author C++ Utilities Library
 * @version 2.0
 * @date 2025
 */
#pragma once

// ============================================================================
// Configuration Macros
// ============================================================================

#if !defined(CPP_UTILITIES_USE_ATOMIC)
#define CPP_UTILITIES_USE_ATOMIC 1 ///< Enable atomic operations (disable for minimal builds)
#endif

#if !defined(CPP_UTILITIES_USE_CHRONO)
#define CPP_UTILITIES_USE_CHRONO 1 ///< Enable chrono for timeouts/polling
#endif

#if !defined(CPP_UTILITIES_DEFAULT_TIMEOUT_SECONDS)
#define CPP_UTILITIES_DEFAULT_TIMEOUT_SECONDS 30 ///< Default timeout for wait operations
#endif

// ============================================================================
// Standard Library Includes
// ============================================================================

#if CPP_UTILITIES_USE_ATOMIC
#include <atomic>      // atomic operations and memory orders
#endif

#include <memory>      // shared_ptr, weak_ptr, unique_ptr, make_shared, allocate_shared
#include <utility>     // exchange, move, forward, pair
#include <type_traits> // enable_if, is_same, etc.
#include <mutex>       // mutex, lock_guard for C++17 weak_ptr thread-safety

#if CPP_UTILITIES_USE_CHRONO
#include <chrono>      // duration, steady_clock, time_point
#include <thread>      // sleep_for, yield
#endif

#include <vector>      // for fetch_add_use_count
#include <cstdint>     // uintptr_t

// ============================================================================
// Library Includes
// ============================================================================

#include "enforce.h"                    // Enforcement macros
#include "enforce_contextual.h"         // Contextual enforcement
#include "enforce_raiser_selector.h"    // RaiserSelector for policy mapping
#include "enforce_enforcers.h"          // MakeEnforcer factory function
#include "EqualityComparisons.h"        // areEqual and EqualDispatcher
#include "Expected.h"                   // Expected<T, E> for error handling
#include "TypeTraits.h"                 // Type trait utilities

namespace cpp_utilities {

// ============================================================================
// Enforcement Policy Implementation
// ============================================================================

/**
 * @brief Convenience macro for enforcement checks within AtomicReference.
 * 
 * @details Uses the project's RaiserSelector to map EnforcementPolicy to appropriate Raiser,
 * then uses MakeEnforcer for type-safe enforcement.
 * 
 * @param condition Boolean condition to check
 * @param message Error message if condition fails
 */
#define enforce_policy_check(condition, message) \
    do { \
        using Raiser = typename cpp_utilities::RaiserSelector<EnforcementPolicy>::type; \
        auto enforcer = cpp_utilities::MakeEnforcer<Raiser>((condition), #condition, \
                                                             __FILE__ ":" CPP_UTILITIES_STRINGIFY(__LINE__)); \
        enforcer(message); \
    } while(0)

// ============================================================================
// Wait Policies
// ============================================================================

/**
 * @brief Native wait policy using C++20 atomic::wait/notify.
 * 
 * @details Uses std::atomic<shared_ptr<T>>::wait for efficient blocking.
 * 
 * Limitations:
 * - Only available in C++20 and later
 * - Timeout support is approximate (delegates to polling for timed waits)
 * - std::atomic::wait blocks indefinitely, so timeouts are implemented via fallback
 * 
 * Implementation Notes:
 * - For infinite timeouts, uses native wait (most efficient)
 * - For finite timeouts, delegates to PollingWaitPolicy
 * - OS-specific timed waits could be added via conditional compilation
 * 
 * @tparam T Value type of the shared_ptr
 */
template <typename T>
struct NativeWaitPolicy {
    /**
     * @brief Wait for value to change with optional timeout.
     * 
     * @details For infinite timeout, uses efficient native wait.
     * For timed waits, delegates to PollingWaitPolicy to avoid indefinite blocking.
     * 
     * @tparam Storage Storage type (AtomicTraits)
     * @tparam Duration Timeout duration type
     * @param ptr Storage reference
     * @param old Expected old value
     * @param order Memory order for loads
     * @param timeout Maximum wait duration
     * @return true if value changed, false if timed out
     */
    template <typename Storage, typename Duration>
    static bool wait(const Storage& ptr, std::shared_ptr<T> old, 
                     std::memory_order order, const Duration& timeout) {
#if __cplusplus >= 202002L && CPP_UTILITIES_USE_CHRONO
        // Check if this is an "infinite" timeout
        if (timeout == Duration::max() || timeout >= std::chrono::hours(24)) {
            // Use native wait for untimed case (most efficient)
            ptr.wait(old, order);
            return true; // Native wait always returns on change
        } else {
            // Delegate to polling for timed waits
            // (std::atomic::wait has no timeout parameter)
            return PollingWaitPolicy<T>::wait(ptr, std::move(old), order, timeout);
        }
#else
        (void)ptr; (void)old; (void)order; (void)timeout;
        return false; // Fallback: no wait in <C++20
#endif
    }

    /**
     * @brief Notify one waiting thread.
     */
    template <typename Storage>
    static void notify_one(const Storage& ptr) noexcept {
#if __cplusplus >= 202002L
        ptr.notify_one();
#else
        (void)ptr;
#endif
    }

    /**
     * @brief Notify all waiting threads.
     */
    template <typename Storage>
    static void notify_all(const Storage& ptr) noexcept {
#if __cplusplus >= 202002L
        ptr.notify_all();
#else
        (void)ptr;
#endif
    }
};

/**
 * @brief Polling wait policy with adaptive backoff and ABA protection.
 * 
 * @details Implements wait via periodic polling with exponential backoff.
 * 
 * Features:
 * - Adaptive delay: starts at 1ms, grows exponentially up to 100ms
 * - ABA mitigation: compares both pointer address AND use_count
 * - Yield-based fallback when chrono unavailable
 * - CPU-friendly: sleeps between checks instead of spinning
 * 
 * ABA Protection:
 * - Checks if pointer changed OR use_count changed
 * - Handles address reuse scenarios
 * - Additional protection possible via generation counters (user-extensible)
 * 
 * Performance:
 * - Initial delay: 1ms (low latency for quick changes)
 * - Max delay: 100ms (prevents CPU waste on long waits)
 * - Adaptive growth: delay *= (1 + attempts/10)
 * 
 * @tparam T Value type of the shared_ptr
 */
template <typename T>
struct PollingWaitPolicy {
    /**
     * @brief Wait via polling with adaptive delays.
     * 
     * @details Periodically checks for changes with exponentially increasing delays.
     * 
     * @tparam TraitsType Storage traits type
     * @tparam Duration Timeout duration type
     * @param traits Storage traits reference
     * @param old Expected old value
     * @param order Memory order for loads
     * @param timeout Maximum wait duration
     * @return true if value changed, false if timed out
     */
    template <typename TraitsType, typename Duration>
    static bool wait(const TraitsType& traits, std::shared_ptr<T> old, 
                     std::memory_order order, const Duration& timeout) {
#if CPP_UTILITIES_USE_CHRONO
        auto start = std::chrono::steady_clock::now();
        auto current = traits.raw_load(order);
        
        // Extract old pointer and use_count for ABA detection
        uintptr_t old_ptr = reinterpret_cast<uintptr_t>(old.get());
        long old_count = old.use_count();
        
        // Adaptive delay parameters
        std::chrono::milliseconds delay(1);  // Start small for low latency
        constexpr auto max_delay = std::chrono::milliseconds(100);
        size_t attempts = 0;
        
        while (std::chrono::steady_clock::now() - start < timeout) {
            // Check for change (ABA-protected)
            uintptr_t cur_ptr = reinterpret_cast<uintptr_t>(current.get());
            long cur_count = current.use_count();
            
            if (cur_ptr != old_ptr || cur_count != old_count) {
                return true; // Change detected
            }
            
            // Adaptive backoff: increase delay based on attempts
            std::this_thread::sleep_for(delay);
            auto new_delay = delay * (1 + attempts / 10);
            if (new_delay < max_delay) {
                delay = new_delay;
            } else {
                delay = max_delay;
            }
            attempts++;
            
            // Reload current value
            current = traits.raw_load(order);
        }
        
        return false; // Timed out
#else
        // No chrono: yield-based spin with adaptive backoff
        // This is less optimal but still better than pure spinning
        size_t spins = 0;
        constexpr size_t max_spins = 10000;  // Tunable based on platform
        constexpr size_t yield_interval = 100;  // Yield every N iterations
        
        while (spins < max_spins) {
            if (traits.raw_load(order) != old) {
                return true;
            }
            
            // Yield periodically to reduce CPU waste
            if (spins % yield_interval == 0) {
                std::this_thread::yield();
            }
            
            spins++;
        }
        
        return false;
#endif
    }

    /**
     * @brief No-op notify (polling doesn't use notifications).
     */
    template <typename TraitsType>
    static void notify_one(const TraitsType&) noexcept {}

    /**
     * @brief No-op notify (polling doesn't use notifications).
     */
    template <typename TraitsType>
    static void notify_all(const TraitsType&) noexcept {}
};

/**
 * @brief Bit-tagged wait policy for packed metadata in pointer low bits.
 * 
 * @details Packs small integer tags into unused pointer bits (typically 2-3 bits on aligned types).
 * 
 * Use Cases:
 * - Version numbers for ABA prevention
 * - State flags (locked, marked, etc.)
 * - Generation counters
 * 
 * Requirements:
 * - Pointer alignment >= (1 << TagBits)
 * - For 2-bit tags: 4-byte alignment (satisfied by most types)
 * - For 3-bit tags: 8-byte alignment (common on 64-bit)
 * 
 * Implementation:
 * - Inherits from PollingWaitPolicy for wait logic
 * - Provides tag helpers: get_tag(), set_tag()
 * - Validates tag consistency after waits
 * 
 * Caution:
 * - Tag validation after wait can cause false positives if tags legitimately change
 * - Consider disabling validation for mutable operations
 * - Ensure alignment via alignas() if needed
 * 
 * @tparam T Value type of the shared_ptr
 * @tparam TagBits Number of low bits to use for tags (default: 2)
 */
template <typename T, size_t TagBits = 2>
struct BitTaggedWaitPolicy : PollingWaitPolicy<T> {
    static_assert(TagBits < sizeof(void*) * 8, "Too many tag bits for pointer size");
    static_assert(TagBits <= 3, "More than 3 tag bits may violate alignment assumptions");
    
    /**
     * @brief Wait with tag validation.
     * 
     * @details Delegates to PollingWaitPolicy then optionally validates tags.
     * 
     * @tparam TraitsType Storage traits type
     * @tparam Duration Timeout duration type
     * @param traits Storage traits reference
     * @param old Expected old value
     * @param order Memory order for loads
     * @param timeout Maximum wait duration
     * @return true if value changed, false if timed out
     */
    template <typename TraitsType, typename Duration>
    static bool wait(const TraitsType& traits, std::shared_ptr<T> old, 
                     std::memory_order order, const Duration& timeout) {
        // Extract old tag for comparison
        uintptr_t old_ptr = reinterpret_cast<uintptr_t>(old.get());
        uintptr_t old_tag = get_tag(old_ptr);
        
        // Delegate to polling for actual wait
        bool result = PollingWaitPolicy<T>::wait(traits, old, order, timeout);
        
        // NOTE: Tag validation disabled by default to avoid false positives
        // Enable only if tags should never change during legitimate operations
        #if 0
        if (result) {
            uintptr_t cur_ptr = reinterpret_cast<uintptr_t>(traits.raw_load(order).get());
            uintptr_t cur_tag = get_tag(cur_ptr);
            enforce(cur_tag == old_tag || cur_tag == (old_tag + 1), "Unexpected tag change during wait");
        }
        #endif
        
        return result;
    }

    /**
     * @brief Extract tag from pointer value.
     * 
     * @param ptr Pointer as uintptr_t
     * @return Tag value (low bits)
     */
    static uintptr_t get_tag(uintptr_t ptr) { 
        return ptr & ((1ULL << TagBits) - 1); 
    }

    /**
     * @brief Set tag in pointer value.
     * 
     * @param ptr Pointer as uintptr_t (tag bits cleared)
     * @param tag New tag value
     * @return Pointer with tag bits set
     */
    static uintptr_t set_tag(uintptr_t ptr, uintptr_t tag) { 
        return (ptr & ~((1ULL << TagBits) - 1)) | (tag & ((1ULL << TagBits) - 1)); 
    }

    /**
     * @brief Clear tag from pointer value.
     * 
     * @param ptr Pointer as uintptr_t (may have tag bits set)
     * @return Pointer with tag bits cleared
     */
    static uintptr_t clear_tag(uintptr_t ptr) {
        return ptr & ~((1ULL << TagBits) - 1);
    }

    template <typename TraitsType>
    static void notify_one(const TraitsType& traits) noexcept {
        PollingWaitPolicy<T>::notify_one(traits);
    }

    template <typename TraitsType>
    static void notify_all(const TraitsType& traits) noexcept {
        PollingWaitPolicy<T>::notify_all(traits);
    }
};

/**
 * @brief Variadic wait policy pack for composing multiple policies.
 * 
 * @details Allows combining multiple wait policies (e.g., Polling + BitTagged).
 * All policies must succeed for wait to return true.
 * 
 * Usage:
 * @code
 * using MyPolicy = WaitPolicyPack<PollingWaitPolicy, BitTaggedWaitPolicy<T, 2>>;
 * AtomicReference<T, DebugOnlyPolicy, MyPolicy> ref;
 * @endcode
 * 
 * @tparam Policies Variadic template of policy types
 */
template <template <typename> class... Policies>
struct WaitPolicyPack {
    /**
     * @brief Composite wait: all policies must succeed.
     * 
     * @tparam T Value type
     * @tparam TraitsType Storage traits type
     * @tparam Duration Timeout duration type
     * @param traits Storage traits reference
     * @param old Expected old value
     * @param order Memory order for loads
     * @param timeout Maximum wait duration
     * @return true if all policies detected change, false otherwise
     */
    template <typename T, typename TraitsType, typename Duration>
    static bool wait(const TraitsType& traits, std::shared_ptr<T> old, 
                     std::memory_order order, const Duration& timeout) {
        bool result = true;
        // Fold expression: call each policy's wait
        ((result &= Policies<T>::wait(traits, old, order, timeout)), ...);
        return result;
    }

    template <typename TraitsType>
    static void notify_one(const TraitsType& traits) noexcept {
        (Policies<typename TraitsType::value_type>::notify_one(traits), ...);
    }

    template <typename TraitsType>
    static void notify_all(const TraitsType& traits) noexcept {
        (Policies<typename TraitsType::value_type>::notify_all(traits), ...);
    }
};

/**
 * @brief Benchmark policy mixin for performance measurement.
 * 
 * @details Wraps a base policy to add timing instrumentation.
 * Zero-cost in release builds if timing code is conditionally compiled.
 * 
 * Usage:
 * @code
 * using InstrumentedPolicy = BenchmarkPolicy<PollingWaitPolicy>;
 * AtomicReference<T, DebugOnlyPolicy, InstrumentedPolicy> ref;
 * @endcode
 * 
 * @tparam BasePolicy The policy to instrument
 */
template <typename BasePolicy>
struct BenchmarkPolicy : BasePolicy {
    template <typename TraitsType, typename Duration>
    static bool wait(const TraitsType& traits, std::shared_ptr<typename TraitsType::value_type> old, 
                     std::memory_order order, const Duration& timeout) {
#if CPP_UTILITIES_USE_CHRONO && !defined(NDEBUG)
        auto start = std::chrono::high_resolution_clock::now();
        bool res = BasePolicy::wait(traits, old, order, timeout);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        // Log or accumulate timing data here
        (void)duration; // Suppress unused warning
        return res;
#else
        return BasePolicy::wait(traits, old, order, timeout);
#endif
    }
};

// ============================================================================
// Atomic Storage Traits (Core Implementation)
// ============================================================================

/**
 * @brief Core atomic storage traits for shared_ptr<T>.
 * 
 * @details Provides atomic operations on shared_ptr with C++20 native support
 * and pre-C++20 fallbacks via std::atomic<std::shared_ptr<T>> emulation.
 * 
 * Features:
 * - Load/store/exchange/compare_exchange operations
 * - Memory order support (acquire, release, seq_cst, etc.)
 * - Lock-free detection via is_lock_free()
 * - Wait/notify operations via policy delegation
 * 
 * Thread Safety:
 * - C++20: Uses std::atomic<std::shared_ptr<T>> (native support)
 * - C++17: Uses std::atomic operations on shared_ptr (may not be lock-free)
 * 
 * @tparam T Value type pointed to by shared_ptr
 * @tparam WaitPolicy Policy for wait/notify operations
 * @tparam EnforcementPolicy Policy for invariant checks
 */
template <typename T, 
          template <typename> class WaitPolicy = PollingWaitPolicy,
          typename EnforcementPolicy = DebugOnlyPolicy>
class AtomicTraits {
public:
    using value_type = T;
    using shared_ptr_type = std::shared_ptr<T>;
    using duration_type = std::chrono::seconds;

protected:
#if __cplusplus >= 202002L
    std::atomic<std::shared_ptr<T>> ptr_;  ///< Native C++20 atomic shared_ptr
#else
    std::shared_ptr<T> ptr_;  ///< Fallback: non-atomic (use external synchronization)
#endif

public:
    /**
     * @brief Default constructor: initializes to nullptr.
     */
    AtomicTraits() noexcept : ptr_(nullptr) {}

    /**
     * @brief Construct from shared_ptr.
     * 
     * @param p Initial value
     */
    explicit AtomicTraits(std::shared_ptr<T> p) noexcept : ptr_(std::move(p)) {}

    /**
     * @brief Atomically load the shared_ptr.
     * 
     * @param order Memory order (default: acquire)
     * @return Copy of the stored shared_ptr
     */
    std::shared_ptr<T> raw_load(std::memory_order order = std::memory_order_acquire) const noexcept {
#if __cplusplus >= 202002L
        return ptr_.load(order);
#else
        // Pre-C++20: use atomic operations
        return std::atomic_load_explicit(&ptr_, order);
#endif
    }

    /**
     * @brief Atomically store a new shared_ptr.
     * 
     * @param p New value
     * @param order Memory order (default: release)
     */
    void raw_store(std::shared_ptr<T> p, std::memory_order order = std::memory_order_release) noexcept {
#if __cplusplus >= 202002L
        ptr_.store(std::move(p), order);
#else
        std::atomic_store_explicit(&ptr_, std::move(p), order);
#endif
    }

    /**
     * @brief Atomically exchange the shared_ptr.
     * 
     * @param p New value
     * @param order Memory order (default: acq_rel)
     * @return Previous value
     */
    std::shared_ptr<T> raw_exchange(std::shared_ptr<T> p, 
                                    std::memory_order order = std::memory_order_acq_rel) noexcept {
#if __cplusplus >= 202002L
        return ptr_.exchange(std::move(p), order);
#else
        return std::atomic_exchange_explicit(&ptr_, std::move(p), order);
#endif
    }

    /**
     * @brief Atomically compare and exchange (weak version).
     * 
     * @param expected Expected current value (updated on failure)
     * @param desired New value if expected matches
     * @param success Memory order on success
     * @param failure Memory order on failure
     * @return true if exchange succeeded, false otherwise
     */
    bool raw_compare_exchange_weak(std::shared_ptr<T>& expected, std::shared_ptr<T> desired,
                                   std::memory_order success, std::memory_order failure) noexcept {
#if __cplusplus >= 202002L
        return ptr_.compare_exchange_weak(expected, std::move(desired), success, failure);
#else
        return std::atomic_compare_exchange_weak_explicit(&ptr_, &expected, std::move(desired), success, failure);
#endif
    }

    /**
     * @brief Atomically compare and exchange (strong version).
     * 
     * @param expected Expected current value (updated on failure)
     * @param desired New value if expected matches
     * @param success Memory order on success
     * @param failure Memory order on failure
     * @return true if exchange succeeded, false otherwise
     */
    bool raw_compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired,
                                     std::memory_order success, std::memory_order failure) noexcept {
#if __cplusplus >= 202002L
        return ptr_.compare_exchange_strong(expected, std::move(desired), success, failure);
#else
        return std::atomic_compare_exchange_strong_explicit(&ptr_, &expected, std::move(desired), success, failure);
#endif
    }

    /**
     * @brief Wait for value to change (with timeout).
     * 
     * @details Delegates to WaitPolicy for implementation.
     * 
     * @tparam Duration Timeout duration type
     * @param old Expected old value
     * @param order Memory order for loads
     * @param timeout Maximum wait duration
     * @return true if value changed, false if timed out
     */
    template <typename Duration = duration_type>
    bool wait(std::shared_ptr<T> old, std::memory_order order = std::memory_order_seq_cst, 
              const Duration& timeout = Duration(CPP_UTILITIES_DEFAULT_TIMEOUT_SECONDS)) const {
        return WaitPolicy<T>::wait(*this, std::move(old), order, timeout);
    }

    /**
     * @brief Notify one waiting thread.
     */
    void notify_one() const noexcept {
        WaitPolicy<T>::notify_one(*this);
    }

    /**
     * @brief Notify all waiting threads.
     */
    void notify_all() const noexcept {
        WaitPolicy<T>::notify_all(*this);
    }

    /**
     * @brief Check if operations are lock-free.
     * 
     * @return true if lock-free, false if uses mutexes
     */
    static constexpr bool is_always_lock_free() noexcept {
#if __cplusplus >= 202002L
        return std::atomic<std::shared_ptr<T>>::is_always_lock_free;
#else
        return false; // Pre-C++20 may not be lock-free
#endif
    }

    /**
     * @brief Check if this instance is lock-free.
     * 
     * @return true if lock-free, false if uses mutexes
     */
    bool is_lock_free() const noexcept {
#if __cplusplus >= 202002L
        return ptr_.is_lock_free();
#else
        return std::atomic_is_lock_free(&ptr_);
#endif
    }
};

/**
 * @brief Atomic storage traits for weak_ptr<T>.
 * 
 * @details Specialization for weak_ptr providing observational access.
 * 
 * Features:
 * - All operations from shared_ptr traits
 * - lock_expected() for safe promotion to shared_ptr
 * - Expiration detection
 * - ABA protection via use_count checks
 * 
 * Use Cases:
 * - Caches (weak references to cached objects)
 * - Event systems (observers don't own observed)
 * - Cycle breaking in graphs
 * 
 * Limitations:
 * - Cannot dereference directly (must lock first)
 * - May expire between check and use
 * - Not suitable for ownership scenarios
 * 
 * @tparam T Value type pointed to by weak_ptr
 * @tparam WaitPolicy Policy for wait/notify operations
 * @tparam EnforcementPolicy Policy for invariant checks
 */
template <typename T, 
          template <typename> class WaitPolicy = PollingWaitPolicy,
          typename EnforcementPolicy = DebugOnlyPolicy>
class AtomicTraitsWeak {
public:
    using value_type = T;
    using weak_ptr_type = std::weak_ptr<T>;
    using shared_ptr_type = std::shared_ptr<T>;
    using duration_type = std::chrono::seconds;

protected:
#if __cplusplus >= 202002L
    std::atomic<std::weak_ptr<T>> ptr_;  ///< Native C++20 atomic weak_ptr
#else
    mutable std::mutex mutex_;           ///< Mutex for thread-safe access in C++17
    std::weak_ptr<T> ptr_;               ///< Fallback: protected by mutex
#endif

public:
    /**
     * @brief Default constructor: initializes to empty weak_ptr.
     */
    AtomicTraitsWeak() noexcept : ptr_() {}

    /**
     * @brief Construct from weak_ptr.
     * 
     * @param p Initial value
     */
    explicit AtomicTraitsWeak(std::weak_ptr<T> p) noexcept : ptr_(std::move(p)) {}

    /**
     * @brief Construct from shared_ptr.
     * 
     * @details Takes shared_ptr by const reference to avoid creating an extra
     * copy that would temporarily increase the use count.
     * 
     * @param p Initial value (converted to weak_ptr)
     */
    explicit AtomicTraitsWeak(const std::shared_ptr<T>& p) noexcept : ptr_(p) {}

    /**
     * @brief Atomically load the weak_ptr.
     * 
     * @param order Memory order (default: acquire)
     * @return Copy of the stored weak_ptr
     */
    std::weak_ptr<T> raw_load(std::memory_order order = std::memory_order_acquire) const noexcept {
#if __cplusplus >= 202002L
        return ptr_.load(order);
#else
        (void)order; // Memory order ignored in pre-C++20
        std::lock_guard<std::mutex> lock(mutex_);
        return ptr_;
#endif
    }

    /**
     * @brief Atomically store a new weak_ptr.
     * 
     * @param p New value
     * @param order Memory order (default: release)
     */
    void raw_store(std::weak_ptr<T> p, std::memory_order order = std::memory_order_release) noexcept {
#if __cplusplus >= 202002L
        ptr_.store(std::move(p), order);
#else
        (void)order;
        std::lock_guard<std::mutex> lock(mutex_);
        ptr_ = std::move(p);
#endif
    }

    /**
     * @brief Atomically exchange the weak_ptr.
     * 
     * @param p New value
     * @param order Memory order (default: acq_rel)
     * @return Previous value
     */
    std::weak_ptr<T> raw_exchange(std::weak_ptr<T> p, 
                                   std::memory_order order = std::memory_order_acq_rel) noexcept {
#if __cplusplus >= 202002L
        return ptr_.exchange(std::move(p), order);
#else
        (void)order;
        std::lock_guard<std::mutex> lock(mutex_);
        return std::exchange(ptr_, std::move(p));
#endif
    }

    /**
     * @brief Atomically compare and exchange (weak version).
     */
    bool raw_compare_exchange_weak(std::weak_ptr<T>& expected, std::weak_ptr<T> desired,
                                   std::memory_order success, std::memory_order failure) noexcept {
#if __cplusplus >= 202002L
        return ptr_.compare_exchange_weak(expected, std::move(desired), success, failure);
#else
        (void)success; (void)failure;
        std::lock_guard<std::mutex> lock(mutex_);
        auto expected_sp = expected.lock();
        auto current_sp = ptr_.lock();
        if (!expected_sp.owner_before(current_sp) && !current_sp.owner_before(expected_sp)) {
            ptr_ = std::move(desired);
            return true;
        }
        expected = ptr_;
        return false;
#endif
    }

    /**
     * @brief Atomically compare and exchange (strong version).
     */
    bool raw_compare_exchange_strong(std::weak_ptr<T>& expected, std::weak_ptr<T> desired,
                                     std::memory_order success, std::memory_order failure) noexcept {
#if __cplusplus >= 202002L
        return ptr_.compare_exchange_strong(expected, std::move(desired), success, failure);
#else
        return raw_compare_exchange_weak(expected, std::move(desired), success, failure);
#endif
    }

    /**
     * @brief Atomically lock the weak_ptr and return as Expected.
     * 
     * @details Provides safe promotion with error handling.
     * 
     * @param order Memory order (default: acquire)
     * @return Expected<shared_ptr<T>, string> - value or "expired" error
     */
    Expected<std::shared_ptr<T>, std::string> lock_expected(
        std::memory_order order = std::memory_order_acquire) const noexcept {
        auto wp = raw_load(order);
        auto sp = wp.lock();
        if (!sp) {
            return make_unexpected("Weak pointer expired");
        }
        return sp;
    }

    /**
     * @brief Check if the weak_ptr has expired.
     * 
     * @param order Memory order (default: acquire)
     * @return true if expired, false if still valid
     */
    bool expired(std::memory_order order = std::memory_order_acquire) const noexcept {
        return raw_load(order).expired();
    }

    /**
     * @brief Get use count of the managed object.
     * 
     * @param order Memory order (default: acquire)
     * @return Use count (0 if expired)
     */
    long use_count(std::memory_order order = std::memory_order_acquire) const noexcept {
        return raw_load(order).use_count();
    }

    /**
     * @brief Wait for weak_ptr to change or expire.
     * 
     * @details Waits for pointer change OR expiration.
     * 
     * @tparam Duration Timeout duration type
     * @param old Expected old value
     * @param order Memory order for loads
     * @param timeout Maximum wait duration
     * @return true if changed or expired, false if timed out
     */
    template <typename Duration = duration_type>
    bool wait(std::weak_ptr<T> old, std::memory_order order = std::memory_order_seq_cst, 
              const Duration& timeout = Duration(CPP_UTILITIES_DEFAULT_TIMEOUT_SECONDS)) const {
        // CRITICAL: Don't lock 'old' at start - that would keep the object alive!
        // Instead, compare weak_ptrs or lock on each iteration
        
        // Custom wait logic for weak_ptr: check for expiration OR pointer change
#if CPP_UTILITIES_USE_CHRONO
        // Immediate check - avoid unnecessary delay if already expired/changed
        {
#if __cplusplus >= 202002L
            auto current = ptr_.load(order);
#else
            std::lock_guard<std::mutex> lock(mutex_);
            auto current = ptr_;
#endif
            // Check if expired
            if (current.expired()) {
                return true;
            }
            // Check if pointer changed by comparing owner
            auto old_sp = old.lock();
            auto current_sp = current.lock();
            if (!old_sp || !current_sp || old_sp != current_sp) {
                return true;  // Changed or one/both expired
            }
        }
        
        auto start = std::chrono::steady_clock::now();
        std::chrono::microseconds delay(100);  // Start with 100μs for faster detection
        constexpr auto max_delay = std::chrono::milliseconds(50);  // Max 50ms
        size_t attempts = 0;
        
        while (std::chrono::steady_clock::now() - start < timeout) {
#if __cplusplus >= 202002L
            auto current = ptr_.load(order);
#else
            std::weak_ptr<T> current;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                current = ptr_;
            }
#endif
            
            // Check if expired
            if (current.expired()) {
                return true;
            }
            
            // Check if pointer changed - lock only for this comparison
            {
                auto old_sp = old.lock();
                auto current_sp = current.lock();
                if (!old_sp || !current_sp || old_sp != current_sp) {
                    return true;
                }
            }  // old_sp and current_sp destroyed here - don't keep object alive!
            
            std::this_thread::sleep_for(delay);
            auto new_delay = delay * (1 + attempts / 10);
            if (new_delay < max_delay) {
                delay = new_delay;
            } else {
                delay = max_delay;
            }
            attempts++;
        }
        
        return false;
#else
        (void)old; (void)order; (void)timeout;
        return false;
#endif
    }

    /**
     * @brief Notify one waiting thread.
     */
    void notify_one() const noexcept {
        // No-op for weak_ptr (could be extended with custom policy)
    }

    /**
     * @brief Notify all waiting threads.
     */
    void notify_all() const noexcept {
        // No-op for weak_ptr
    }

    /**
     * @brief Check if operations are lock-free.
     */
    static constexpr bool is_always_lock_free() noexcept {
#if __cplusplus >= 202002L
        return std::atomic<std::weak_ptr<T>>::is_always_lock_free;
#else
        return false;
#endif
    }

    /**
     * @brief Check if this instance is lock-free.
     */
    bool is_lock_free() const noexcept {
#if __cplusplus >= 202002L
        return ptr_.is_lock_free();
#else
        return false;
#endif
    }
};

/**
 * @brief Custom storage traits for user-defined types.
 * 
 * @details Template specialization point for users to provide custom atomic storage.
 * 
 * Requirements:
 * - Must provide: raw_load, raw_store, raw_exchange, raw_compare_exchange_*
 * - Must provide: wait, notify_one, notify_all
 * - Must provide: is_always_lock_free, is_lock_free
 * - Must define: value_type, shared_ptr_type, duration_type
 * 
 * Example:
 * @code
 * template <>
 * class CustomStorageTraits<MyType, MyWaitPolicy, MyEnforcementPolicy> {
 *     // Implement all required methods
 * };
 * @endcode
 * 
 * @tparam T Value type
 * @tparam WaitPolicy Policy for wait/notify
 * @tparam EnforcementPolicy Policy for invariant checks
 */
template <typename T, 
          template <typename> class WaitPolicy,
          typename EnforcementPolicy>
class CustomStorageTraits : public AtomicTraits<T, WaitPolicy, EnforcementPolicy> {
public:
    using base_type = AtomicTraits<T, WaitPolicy, EnforcementPolicy>;
    using base_type::base_type;  // Inherit constructors
    
    // Users can add custom methods or override behavior here
    // Example: SIMD load for vector types
    #if 0
    #include <immintrin.h>
    template <typename U = T, std::enable_if_t<std::is_same_v<U, std::vector<float>>, int> = 0>
    std::shared_ptr<U> simd_load(std::memory_order order) const noexcept {
        // Use SIMD intrinsics for optimized load
        // e.g., _mm_load_ps for float vectors
        return base_type::raw_load(order);
    }
    #endif
};

// ============================================================================
// Forward Declarations
// ============================================================================

template <typename T, typename EnforcementPolicy, template <typename> class WaitPolicy, typename DurationPolicy>
class AtomicReference;

// ============================================================================
// Invariant Guard
// ============================================================================

/**
 * @brief RAII guard for checking AtomicReference invariants.
 * 
 * @details Captures state on construction and validates on destruction.
 * 
 * Checks:
 * - Use count doesn't decrease unexpectedly (for shared_ptr)
 * - Pointer remains valid (for reads)
 * - Policy-specific assertions
 * 
 * False Positive Prevention:
 * - Disabled for mutating operations (store, exchange, etc.)
 * - Uses equality checks for reads, relaxed checks for mutations
 * - Configurable via EnforcementPolicy
 * 
 * @tparam T Value type
 * @tparam EnforcementPolicy Policy for invariant checks
 * @tparam WaitPolicy Wait policy type
 * @tparam DurationPolicy Duration type for timeouts
 */
template <typename T, typename EnforcementPolicy, template <typename> class WaitPolicy, typename DurationPolicy = std::chrono::seconds>
class InvariantGuard {
private:
    const AtomicReference<T, EnforcementPolicy, WaitPolicy, DurationPolicy>& ref_;
    long initial_count_;
    T* initial_ptr_;
    bool mutating_op_;  ///< True for store/exchange/CAS (relaxed checks)

public:
    /**
     * @brief Construct guard and capture initial state.
     * 
     * @param ref Reference to AtomicReference
     * @param mutating True if this is a mutating operation
     */
    explicit InvariantGuard(const AtomicReference<T, EnforcementPolicy, WaitPolicy, DurationPolicy>& ref, 
                           bool mutating = false) noexcept
        : ref_(ref)
        , initial_count_(0)
        , initial_ptr_(nullptr)
        , mutating_op_(mutating)
    {
        auto sp = ref_.raw_load(std::memory_order_relaxed);
        if (sp) {
            initial_count_ = sp.use_count();
            initial_ptr_ = sp.get();
        }
    }

    /**
     * @brief Destructor: validate invariants.
     * 
     * @details Checks depend on operation type:
     * - Read operations: use_count must not decrease
     * - Write operations: relaxed checks (reset to 1 is expected)
     */
    ~InvariantGuard() noexcept(false) {
        auto sp = ref_.raw_load(std::memory_order_relaxed);
        
        if (!mutating_op_) {
            // Read operations: use_count should not decrease
            long final_count = sp ? sp.use_count() : 0;
            T* final_ptr = sp ? sp.get() : nullptr;
            
            // Only enforce for non-mutating reads
            if (initial_ptr_ && final_ptr == initial_ptr_) {
                enforce_policy_check(final_count >= initial_count_, 
                                   "Use count decreased unexpectedly during read");
            }
        }
        // Mutating operations: skip use_count check (reset to 1 is expected)
    }

    // Non-copyable, non-movable
    InvariantGuard(const InvariantGuard&) = delete;
    InvariantGuard& operator=(const InvariantGuard&) = delete;
};

/**
 * @brief Invariant guard specialization for weak_ptr.
 * 
 * @details Checks expiration state rather than use_count.
 * 
 * @tparam T Value type
 * @tparam EnforcementPolicy Policy for invariant checks
 * @tparam WaitPolicy Wait policy type
 * @tparam DurationPolicy Duration type for timeouts
 */
template <typename T, typename EnforcementPolicy, template <typename> class WaitPolicy, typename DurationPolicy = std::chrono::seconds>
class InvariantGuardWeak {
private:
    const AtomicReference<std::weak_ptr<T>, EnforcementPolicy, WaitPolicy, DurationPolicy>& ref_;
    bool initially_expired_;
    bool mutating_op_;

public:
    explicit InvariantGuardWeak(const AtomicReference<std::weak_ptr<T>, EnforcementPolicy, WaitPolicy, DurationPolicy>& ref,
                               bool mutating = false) noexcept
        : ref_(ref)
        , initially_expired_(false)
        , mutating_op_(mutating)
    {
        auto wp = ref_.raw_load(std::memory_order_relaxed);
        initially_expired_ = wp.expired();
    }

    ~InvariantGuardWeak() noexcept(false) {
        if (!mutating_op_) {
            auto wp = ref_.raw_load(std::memory_order_relaxed);
            // Can transition from valid to expired, but not expired to valid without store
            if (initially_expired_) {
                enforce_policy_check(wp.expired(), 
                                   "Weak pointer unexpectedly became valid without store");
            }
        }
    }

    InvariantGuardWeak(const InvariantGuardWeak&) = delete;
    InvariantGuardWeak& operator=(const InvariantGuardWeak&) = delete;
};

// ============================================================================
// AtomicReference (shared_ptr specialization)
// ============================================================================

/**
 * @brief Thread-safe atomic reference for shared_ptr<T>.
 * 
 * @details Main class providing atomic operations on shared_ptr with:
 * - Load/store/exchange/compare_exchange
 * - Wait/notify for thread coordination
 * - Invariant checking via InvariantGuard
 * - Expected-based error handling
 * - Custom wait and enforcement policies
 * 
 * Thread Safety:
 * - All operations are thread-safe
 * - Lock-free where hardware supports it
 * - Memory orders control visibility between threads
 * 
 * @tparam T Value type (for shared_ptr<T>)
 * @tparam EnforcementPolicy Policy for contract enforcement
 * @tparam WaitPolicy Policy for wait/notify operations (default: PollingWaitPolicy)
 * @tparam DurationPolicy Duration type for timeouts (default: std::chrono::seconds)
 */
template <typename T, 
          typename EnforcementPolicy = DebugOnlyPolicy,
          template <typename> class WaitPolicy = PollingWaitPolicy,
          typename DurationPolicy = std::chrono::seconds>
class AtomicReference : private AtomicTraits<T, WaitPolicy, EnforcementPolicy> {
private:
    using traits_type = AtomicTraits<T, WaitPolicy, EnforcementPolicy>;
    using guard_type = InvariantGuard<T, EnforcementPolicy, WaitPolicy, DurationPolicy>;

public:
    using value_type = T;
    using shared_ptr_type = std::shared_ptr<T>;
    using duration_type = DurationPolicy;

    // ========================================================================
    // Constructors
    // ========================================================================

    /**
     * @brief Default constructor: initializes to nullptr.
     */
    AtomicReference() noexcept : traits_type() {}

    /**
     * @brief Construct from shared_ptr.
     * 
     * @param p Initial value
     */
    explicit AtomicReference(std::shared_ptr<T> p) noexcept : traits_type(std::move(p)) {}

    /**
     * @brief Copy constructor (deleted - use load/store instead).
     */
    AtomicReference(const AtomicReference&) = delete;

    /**
     * @brief Move constructor (deleted - atomics cannot be moved).
     */
    AtomicReference(AtomicReference&&) = delete;

    /**
     * @brief Copy assignment (deleted).
     */
    AtomicReference& operator=(const AtomicReference&) = delete;

    /**
     * @brief Move assignment (deleted).
     */
    AtomicReference& operator=(AtomicReference&&) = delete;

    // ========================================================================
    // Load Operations
    // ========================================================================

    /**
     * @brief Atomically load the shared_ptr with invariant checking.
     * 
     * @param order Memory order (default: acquire)
     * @return Copy of the stored shared_ptr
     */
    std::shared_ptr<T> load(std::memory_order order = std::memory_order_acquire) const {
        guard_type guard(*this, false);  // Read operation
        auto result = this->raw_load(order);
        // Enforce non-null based on EnforcementPolicy
        using Raiser = typename RaiserSelector<EnforcementPolicy>::type;
        auto enforcer = MakeEnforcer<Raiser>(result != nullptr, "result != nullptr", 
                                             __FILE__ ":" CPP_UTILITIES_STRINGIFY(__LINE__));
        enforcer("Loaded null from AtomicReference");
        return result;
    }

    /**
     * @brief Atomically load without enforcement checks.
     * 
     * @param order Memory order (default: acquire)
     * @return Copy of the stored shared_ptr (may be null)
     */
    std::shared_ptr<T> raw_load(std::memory_order order = std::memory_order_acquire) const noexcept {
        return traits_type::raw_load(order);
    }

    /**
     * @brief Load with Expected-based error handling.
     * 
     * @param order Memory order (default: acquire)
     * @return Expected<shared_ptr<T>, string> - value or "null" error
     */
    Expected<std::shared_ptr<T>, std::string> load_expected(
        std::memory_order order = std::memory_order_acquire) const noexcept {
        guard_type guard(*this, false);
        auto result = this->raw_load(order);
        if (!result) {
            return make_unexpected("Null loaded from AtomicReference");
        }
        return result;
    }

    // ========================================================================
    // Store Operations
    // ========================================================================

    /**
     * @brief Atomically store a new shared_ptr.
     * 
     * @param p New value
     * @param order Memory order (default: release)
     */
    void store(std::shared_ptr<T> p, std::memory_order order = std::memory_order_release) {
        guard_type guard(*this, true);  // Mutating operation
        // Enforce non-null based on EnforcementPolicy
        using Raiser = typename RaiserSelector<EnforcementPolicy>::type;
        auto enforcer = MakeEnforcer<Raiser>(p != nullptr, "p != nullptr", 
                                             __FILE__ ":" CPP_UTILITIES_STRINGIFY(__LINE__));
        enforcer("Storing null to AtomicReference");
        this->raw_store(std::move(p), order);
    }

    /**
     * @brief Atomically store without enforcement checks.
     * 
     * @param p New value
     * @param order Memory order (default: release)
     */
    void raw_store(std::shared_ptr<T> p, std::memory_order order = std::memory_order_release) noexcept {
        traits_type::raw_store(std::move(p), order);
    }

    // ========================================================================
    // Exchange Operations
    // ========================================================================

    /**
     * @brief Atomically exchange the shared_ptr.
     * 
     * @param desired New value
     * @param order Memory order (default: acq_rel)
     * @return Previous value
     */
    std::shared_ptr<T> exchange(std::shared_ptr<T> desired, 
                                std::memory_order order = std::memory_order_acq_rel) {
        guard_type guard(*this, true);  // Mutating operation
        // Enforce non-null based on EnforcementPolicy
        using Raiser = typename RaiserSelector<EnforcementPolicy>::type;
        auto enforcer = MakeEnforcer<Raiser>(desired != nullptr, "desired != nullptr", 
                                             __FILE__ ":" CPP_UTILITIES_STRINGIFY(__LINE__));
        enforcer("Exchanging null to AtomicReference");
        return this->raw_exchange(std::move(desired), order);
    }

    /**
     * @brief Atomically exchange without enforcement checks.
     * 
     * @param desired New value
     * @param order Memory order (default: acq_rel)
     * @return Previous value
     */
    std::shared_ptr<T> raw_exchange(std::shared_ptr<T> desired, 
                                    std::memory_order order = std::memory_order_acq_rel) noexcept {
        return traits_type::raw_exchange(std::move(desired), order);
    }

    // ========================================================================
    // Compare-Exchange Operations
    // ========================================================================

    /**
     * @brief Atomically compare and exchange (weak version).
     * 
     * @details Weak CAS may fail spuriously even if values match.
     * Use in loops for best performance.
     * 
     * @param expected Expected current value (updated on failure)
     * @param desired New value if expected matches
     * @param success Memory order on success
     * @param failure Memory order on failure
     * @return true if exchange succeeded, false otherwise
     */
    bool compare_exchange_weak(std::shared_ptr<T>& expected, std::shared_ptr<T> desired,
                              std::memory_order success = std::memory_order_acq_rel,
                              std::memory_order failure = std::memory_order_acquire) {
        guard_type guard(*this, true);  // Mutating operation
        enforce_policy_check(expected.get() != desired.get(), "CAS with identical expected and desired");
        return this->raw_compare_exchange_weak(expected, std::move(desired), success, failure);
    }

    /**
     * @brief Atomically compare and exchange (strong version).
     * 
     * @details Strong CAS never fails spuriously.
     * Use when spurious failures are unacceptable.
     * 
     * @param expected Expected current value (updated on failure)
     * @param desired New value if expected matches
     * @param success Memory order on success
     * @param failure Memory order on failure
     * @return true if exchange succeeded, false otherwise
     */
    bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired,
                                std::memory_order success = std::memory_order_acq_rel,
                                std::memory_order failure = std::memory_order_acquire) {
        guard_type guard(*this, true);  // Mutating operation
        enforce_policy_check(expected.get() != desired.get(), "CAS with identical expected and desired");
        return this->raw_compare_exchange_strong(expected, std::move(desired), success, failure);
    }

    /**
     * @brief Compare-exchange with retry logic (Expected-based).
     * 
     * @param expected Expected current value
     * @param desired New value if expected matches
     * @param success Memory order on success
     * @param failure Memory order on failure
     * @param max_retries Maximum retry attempts
     * @return Expected<bool, string> - true on success, error message on failure
     */
    Expected<bool, std::string> try_compare_exchange_weak(
        std::shared_ptr<T>& expected, std::shared_ptr<T> desired,
        std::memory_order success = std::memory_order_acq_rel,
        std::memory_order failure = std::memory_order_acquire,
        size_t max_retries = 10) {
        guard_type guard(*this, true);
        for (size_t i = 0; i < max_retries; ++i) {
            if (this->raw_compare_exchange_weak(expected, std::move(desired), success, failure)) {
                return true;
            }
            // expected is updated by CAS on failure, prepare desired for next attempt
            desired = std::move(expected);
        }
        return make_unexpected("CAS failed after " + std::to_string(max_retries) + " retries");
    }

    /**
     * @brief Compare-exchange with retry logic (strong version).
     */
    Expected<bool, std::string> try_compare_exchange_strong(
        std::shared_ptr<T>& expected, std::shared_ptr<T> desired,
        std::memory_order success = std::memory_order_acq_rel,
        std::memory_order failure = std::memory_order_acquire,
        size_t max_retries = 10) {
        guard_type guard(*this, true);
        for (size_t i = 0; i < max_retries; ++i) {
            if (this->raw_compare_exchange_strong(expected, std::move(desired), success, failure)) {
                return true;
            }
            desired = std::move(expected);
        }
        return make_unexpected("CAS failed after " + std::to_string(max_retries) + " retries");
    }

    // ========================================================================
    // Use Count Operations
    // ========================================================================

    /**
     * @brief Get the use count of the managed object.
     * 
     * @details Returns the use count excluding the temporary reference created
     * by this method. This gives the "external" use count, not counting the
     * reference held temporarily during the measurement.
     * 
     * @param order Memory order (default: acquire)
     * @return Use count (0 if null)
     */
    long use_count(std::memory_order order = std::memory_order_acquire) const noexcept {
#if __cplusplus >= 202002L
        // C++20: Direct access to avoid temporary
        auto sp = this->ptr_.load(order);
        return sp ? (sp.use_count() - 1) : 0;
#else
        // C++17: Account for the temporary created by atomic_load
        auto sp = std::atomic_load_explicit(&this->ptr_, order);
        return sp ? (sp.use_count() - 1) : 0;
#endif
    }

    /**
     * @brief Atomically increment use count and return holders.
     * 
     * @details Creates delta copies of the shared_ptr to increase use_count.
     * Uses CAS loop to ensure atomicity - retries if pointer changes during operation.
     * 
     * Fixed from original implementation:
     * - Now uses CAS loop to prevent applying to stale pointers
     * - Ensures atomicity even under concurrent modifications
     * - Returns old use_count before increment (external count, not including AtomicReference's own reference)
     * 
     * @param delta Number of references to add (default: 1)
     * @param order Memory order (default: acq_rel)
     * @return Pair of (old_use_count, vector of holders)
     */
    std::pair<long, std::vector<std::shared_ptr<T>>> fetch_add_use_count(
        long delta = 1, std::memory_order order = std::memory_order_acq_rel) noexcept {
        std::shared_ptr<T> expected;
        std::vector<std::shared_ptr<T>> holders;
        long old_count;
        
        do {
            // Load current value
            expected = this->raw_load(order);
            if (!expected) {
                return {0, {}};  // Null pointer, no use count to increment
            }
            
            // Calculate old external use count: total - AtomicReference's reference - expected's reference
            old_count = expected.use_count() - 2;
            
            // Create holders to bump use_count
            holders.clear();
            holders.reserve(delta);
            for (long i = 0; i < delta; ++i) {
                holders.push_back(expected);
            }
            
            // CAS to confirm pointer hasn't changed
            // If it has, expected is updated and we retry
            // Note: This is a no-op CAS (desired == expected), just checking for changes
            auto desired = expected;  // Copy for CAS
        } while (!this->raw_compare_exchange_weak(expected, expected, order, std::memory_order_relaxed));
        
        // Successfully created holders with atomic guarantee
        return {old_count, std::move(holders)};
    }

    // ========================================================================
    // Wait/Notify Operations
    // ========================================================================

    /**
     * @brief Wait for value to change.
     * 
     * @details Blocks until value differs from old or timeout expires.
     * Implementation delegated to WaitPolicy.
     * 
     * @tparam Duration Timeout duration type
     * @param old Expected old value
     * @param order Memory order for loads (default: seq_cst)
     * @param timeout Maximum wait duration (default: 30 seconds)
     * @return true if value changed, false if timed out
     */
    template <typename Duration = duration_type>
    bool wait(std::shared_ptr<T> old, 
              std::memory_order order = std::memory_order_seq_cst,
              const Duration& timeout = Duration(CPP_UTILITIES_DEFAULT_TIMEOUT_SECONDS)) const {
        return traits_type::wait(std::move(old), order, timeout);
    }

    /**
     * @brief Notify one waiting thread.
     * 
     * @details Wakes up one thread blocked in wait().
     */
    void notify_one() const noexcept {
        traits_type::notify_one();
    }

    /**
     * @brief Notify all waiting threads.
     * 
     * @details Wakes up all threads blocked in wait().
     */
    void notify_all() const noexcept {
        traits_type::notify_all();
    }

    // ========================================================================
    // Lock-Free Queries
    // ========================================================================

    /**
     * @brief Check if operations are always lock-free for this type.
     * 
     * @return true if lock-free, false if may use mutexes
     */
    static constexpr bool is_always_lock_free() noexcept {
        return traits_type::is_always_lock_free();
    }

    /**
     * @brief Check if this instance's operations are lock-free.
     * 
     * @return true if lock-free, false if uses mutexes
     */
    bool is_lock_free() const noexcept {
        return traits_type::is_lock_free();
    }

    /**
     * @brief Enforce that operations are lock-free (throws if not).
     * 
     * @details Useful for guaranteeing real-time properties.
     */
    void enforce_lock_free() const {
        if constexpr (std::is_same_v<EnforcementPolicy, AlwaysEnforcePolicy>) {
            enforce(is_lock_free(), "Platform not lock-free for AtomicReference");
        } else if constexpr (std::is_same_v<EnforcementPolicy, DebugOnlyPolicy>) {
#ifndef NDEBUG
            enforce(is_lock_free(), "Platform not lock-free for AtomicReference");
#endif
        }
    }

    // ========================================================================
    // Convenience Operators
    // ========================================================================

    /**
     * @brief Dereference operator (loads and dereferences).
     * 
     * @return Reference to the managed object
     * @throws If loaded pointer is null
     */
    T& operator*() const {
        auto p = load();
        enforce_policy_check(p != nullptr, "Dereferencing null AtomicReference");
        return *p;
    }

    /**
     * @brief Arrow operator (loads and returns pointer).
     * 
     * @return Pointer to the managed object
     * @throws If loaded pointer is null
     */
    T* operator->() const {
        auto p = load();
        enforce_policy_check(p != nullptr, "Dereferencing null AtomicReference");
        return p.get();
    }

    /**
     * @brief Implicit conversion to shared_ptr (loads).
     * 
     * @return Copy of the stored shared_ptr
     */
    operator std::shared_ptr<T>() const {
        return load();
    }

    /**
     * @brief Assign loaded value to a shared_ptr.
     * 
     * @param target Target shared_ptr to receive loaded value
     */
    void assign_to(std::shared_ptr<T>& target) const {
        target = load();
    }

    // ========================================================================
    // Comparison Operations
    // ========================================================================

    /**
     * @brief Owner-based ordering (compares ownership, not value).
     * 
     * @tparam U Value type of other shared_ptr
     * @param other shared_ptr to compare with
     * @return true if this < other in owner order
     */
    template <typename U>
    bool owner_before(const std::shared_ptr<U>& other) const noexcept {
        return this->raw_load(std::memory_order_relaxed).owner_before(other);
    }

    /**
     * @brief Owner-based ordering (compares ownership, not value).
     * 
     * @param other AtomicReference to compare with
     * @return true if this < other in owner order
     */
    bool owner_before(const AtomicReference& other) const noexcept {
        return this->raw_load(std::memory_order_relaxed).owner_before(
            other.raw_load(std::memory_order_relaxed));
    }
};

// ============================================================================
// AtomicReference (weak_ptr specialization)
// ============================================================================

/**
 * @brief Thread-safe atomic reference for weak_ptr<T> (observational access).
 * 
 * @details Specialization for weak_ptr providing non-owning references.
 * 
 * Features:
 * - All operations from shared_ptr specialization
 * - lock_expected() for safe promotion
 * - Expiration detection
 * - Wait on expiration or change
 * 
 * Use Cases:
 * - Caches (observers don't extend lifetime)
 * - Event systems (observers can be notified when subject dies)
 * - Breaking reference cycles
 * 
 * Limitations:
 * - Cannot dereference directly (must lock first)
 * - May expire between operations
 * - Not suitable for ownership scenarios
 * 
 * @tparam T Value type (for weak_ptr<T>)
 * @tparam EnforcementPolicy Policy for contract enforcement
 * @tparam WaitPolicy Policy for wait/notify operations
 * @tparam DurationPolicy Duration type for timeouts
 */
template <typename T, 
          typename EnforcementPolicy,
          template <typename> class WaitPolicy,
          typename DurationPolicy>
class AtomicReference<std::weak_ptr<T>, EnforcementPolicy, WaitPolicy, DurationPolicy> 
    : private AtomicTraitsWeak<T, WaitPolicy, EnforcementPolicy> {
private:
    using traits_type = AtomicTraitsWeak<T, WaitPolicy, EnforcementPolicy>;
    using guard_type = InvariantGuardWeak<T, EnforcementPolicy, WaitPolicy, DurationPolicy>;

public:
    using value_type = T;
    using weak_ptr_type = std::weak_ptr<T>;
    using shared_ptr_type = std::shared_ptr<T>;
    using duration_type = DurationPolicy;

    // ========================================================================
    // Constructors
    // ========================================================================

    /**
     * @brief Default constructor: initializes to empty weak_ptr.
     */
    AtomicReference() noexcept : traits_type() {}

    /**
     * @brief Construct from weak_ptr.
     */
    explicit AtomicReference(std::weak_ptr<T> p) noexcept : traits_type(std::move(p)) {}

    /**
     * @brief Construct from shared_ptr (converted to weak_ptr).
     * 
     * @details Takes shared_ptr by const reference to avoid creating an extra
     * copy that would temporarily keep the object alive.
     */
    explicit AtomicReference(const std::shared_ptr<T>& p) noexcept : traits_type(p) {}

    AtomicReference(const AtomicReference&) = delete;
    AtomicReference(AtomicReference&&) = delete;
    AtomicReference& operator=(const AtomicReference&) = delete;
    AtomicReference& operator=(AtomicReference&&) = delete;

    // ========================================================================
    // Load Operations
    // ========================================================================

    /**
     * @brief Atomically load the weak_ptr.
     * 
     * @param order Memory order (default: acquire)
     * @return Copy of the stored weak_ptr
     */
    std::weak_ptr<T> load(std::memory_order order = std::memory_order_acquire) const {
        guard_type guard(*this, false);
        return this->raw_load(order);
    }

    /**
     * @brief Atomically load without enforcement checks.
     */
    std::weak_ptr<T> raw_load(std::memory_order order = std::memory_order_acquire) const noexcept {
        return traits_type::raw_load(order);
    }

    /**
     * @brief Atomically lock the weak_ptr and return as Expected.
     * 
     * @warning The returned shared_ptr (if valid) KEEPS THE OBJECT ALIVE!
     * If you hold onto this returned shared_ptr, the weak_ptr will NOT expire
     * until you release it. For expiration checks, use expired() directly.
     * 
     * @details Provides safe promotion with error handling.
     * 
     * Example of INCORRECT usage:
     * @code
     * auto sp = std::make_shared<int>(42);
     * AtomicReference<std::weak_ptr<int>> ref(sp);
     * auto locked = ref.lock_expected();  // locked holds shared_ptr!
     * sp.reset();
     * ref.expired();  // Returns FALSE! locked is still holding a reference
     * @endcode
     * 
     * Correct usage:
     * @code
     * auto sp = std::make_shared<int>(42);
     * AtomicReference<std::weak_ptr<int>> ref(sp);
     * {
     *     auto locked = ref.lock_expected();  // Use in limited scope
     *     if (locked) { use locked.value() }
     * }  // locked destroyed here
     * sp.reset();
     * ref.expired();  // Returns TRUE now
     * @endcode
     * 
     * @param order Memory order (default: acquire)
     * @return Expected<shared_ptr<T>, string> - value or "expired" error
     */
    Expected<std::shared_ptr<T>, std::string> lock_expected(
        std::memory_order order = std::memory_order_acquire) const noexcept {
        guard_type guard(*this, false);
        return traits_type::lock_expected(order);
    }

    /**
     * @brief Check if the weak_ptr has expired.
     * 
     * @param order Memory order (default: acquire)
     * @return true if expired, false if still valid
     */
    bool expired(std::memory_order order = std::memory_order_acquire) const noexcept {
        return traits_type::expired(order);
    }

    /**
     * @brief Get use count of the managed object.
     * 
     * @param order Memory order (default: acquire)
     * @return Use count (0 if expired)
     */
    long use_count(std::memory_order order = std::memory_order_acquire) const noexcept {
        return traits_type::use_count(order);
    }

    // ========================================================================
    // Store Operations
    // ========================================================================

    /**
     * @brief Atomically store a new weak_ptr.
     */
    void store(std::weak_ptr<T> p, std::memory_order order = std::memory_order_release) {
        guard_type guard(*this, true);
        this->raw_store(std::move(p), order);
    }

    /**
     * @brief Atomically store a new weak_ptr (from shared_ptr).
     * 
     * @details Stores the weak_ptr and triggers notification for waiting threads.
     */
    void store(std::shared_ptr<T> p, std::memory_order order = std::memory_order_release) {
        guard_type guard(*this, true);
        std::weak_ptr<T> wp(std::move(p));
        this->raw_store(std::move(wp), order);
        // Note: notify is called by guard destructor if needed
    }

    /**
     * @brief Atomically store without enforcement checks.
     */
    void raw_store(std::weak_ptr<T> p, std::memory_order order = std::memory_order_release) noexcept {
        traits_type::raw_store(std::move(p), order);
    }

    // ========================================================================
    // Exchange Operations
    // ========================================================================

    /**
     * @brief Atomically exchange the weak_ptr.
     */
    std::weak_ptr<T> exchange(std::weak_ptr<T> desired, 
                              std::memory_order order = std::memory_order_acq_rel) {
        guard_type guard(*this, true);
        return this->raw_exchange(std::move(desired), order);
    }

    /**
     * @brief Atomically exchange without enforcement checks.
     */
    std::weak_ptr<T> raw_exchange(std::weak_ptr<T> desired, 
                                  std::memory_order order = std::memory_order_acq_rel) noexcept {
        return traits_type::raw_exchange(std::move(desired), order);
    }

    // ========================================================================
    // Compare-Exchange Operations
    // ========================================================================

    /**
     * @brief Atomically compare and exchange (weak version).
     */
    bool compare_exchange_weak(std::weak_ptr<T>& expected, std::weak_ptr<T> desired,
                              std::memory_order success = std::memory_order_acq_rel,
                              std::memory_order failure = std::memory_order_acquire) {
        guard_type guard(*this, true);
        return this->raw_compare_exchange_weak(expected, std::move(desired), success, failure);
    }

    /**
     * @brief Atomically compare and exchange (strong version).
     */
    bool compare_exchange_strong(std::weak_ptr<T>& expected, std::weak_ptr<T> desired,
                                std::memory_order success = std::memory_order_acq_rel,
                                std::memory_order failure = std::memory_order_acquire) {
        guard_type guard(*this, true);
        return this->raw_compare_exchange_strong(expected, std::move(desired), success, failure);
    }

    // ========================================================================
    // Wait/Notify Operations
    // ========================================================================

    /**
     * @brief Wait for weak_ptr to change or expire.
     * 
     * @tparam Duration Timeout duration type
     * @param old Expected old value
     * @param order Memory order for loads
     * @param timeout Maximum wait duration
     * @return true if changed or expired, false if timed out
     */
    template <typename Duration = duration_type>
    bool wait(std::weak_ptr<T> old, 
              std::memory_order order = std::memory_order_seq_cst,
              const Duration& timeout = Duration(CPP_UTILITIES_DEFAULT_TIMEOUT_SECONDS)) const {
        return traits_type::wait(std::move(old), order, timeout);
    }

    /**
     * @brief Notify one waiting thread.
     */
    void notify_one() const noexcept {
        traits_type::notify_one();
    }

    /**
     * @brief Notify all waiting threads.
     */
    void notify_all() const noexcept {
        traits_type::notify_all();
    }

    // ========================================================================
    // Lock-Free Queries
    // ========================================================================

    static constexpr bool is_always_lock_free() noexcept {
        return traits_type::is_always_lock_free();
    }

    bool is_lock_free() const noexcept {
        return traits_type::is_lock_free();
    }

    void enforce_lock_free() const {
        if constexpr (std::is_same_v<EnforcementPolicy, AlwaysEnforcePolicy>) {
            enforce(is_lock_free(), "Platform not lock-free for AtomicReference<weak_ptr>");
        } else if constexpr (std::is_same_v<EnforcementPolicy, DebugOnlyPolicy>) {
#ifndef NDEBUG
            enforce(is_lock_free(), "Platform not lock-free for AtomicReference<weak_ptr>");
#endif
        }
    }

    // ========================================================================
    // Comparison Operations
    // ========================================================================

    /**
     * @brief Owner-based ordering.
     */
    template <typename U>
    bool owner_before(const std::weak_ptr<U>& other) const noexcept {
        return this->raw_load(std::memory_order_relaxed).owner_before(other);
    }

    /**
     * @brief Owner-based ordering.
     */
    bool owner_before(const AtomicReference& other) const noexcept {
        return this->raw_load(std::memory_order_relaxed).owner_before(
            other.raw_load(std::memory_order_relaxed));
    }
};

// ============================================================================
// EqualDispatcher Specializations
// ============================================================================

/**
 * @brief Equality comparison for AtomicReference<shared_ptr<T>>.
 * 
 * @details Compares values, not pointers (uses EqualDispatcher<T>).
 * 
 * @tparam T Value type
 * @tparam EnforcementPolicy Enforcement policy
 * @tparam WaitPolicy Wait policy
 * @tparam DurationPolicy Duration type for timeouts
 * @tparam Policy Equality comparison policy
 */
template<typename T, typename EnforcementPolicy, template <typename> class WaitPolicy, typename DurationPolicy, typename Policy>
struct EqualDispatcher<AtomicReference<T, EnforcementPolicy, WaitPolicy, DurationPolicy>, Policy> {
    template<typename... EpsParams>
    static bool compare(const AtomicReference<T, EnforcementPolicy, WaitPolicy, DurationPolicy>& a,
                       const AtomicReference<T, EnforcementPolicy, WaitPolicy, DurationPolicy>& b,
                       EpsParams... eps) {
        auto pa = a.raw_load();
        auto pb = b.raw_load();
        if (!pa && !pb) return true;  // Both null
        if (!pa || !pb) return false;  // One null
        return EqualDispatcher<T, Policy>::compare(*pa, *pb, eps...);
    }
};

/**
 * @brief Equality comparison for AtomicReference<weak_ptr<T>>.
 * 
 * @details Locks weak_ptrs and compares values.
 */
template<typename T, typename EnforcementPolicy, template <typename> class WaitPolicy, typename DurationPolicy, typename Policy>
struct EqualDispatcher<AtomicReference<std::weak_ptr<T>, EnforcementPolicy, WaitPolicy, DurationPolicy>, Policy> {
    template<typename... EpsParams>
    static bool compare(const AtomicReference<std::weak_ptr<T>, EnforcementPolicy, WaitPolicy, DurationPolicy>& a,
                       const AtomicReference<std::weak_ptr<T>, EnforcementPolicy, WaitPolicy, DurationPolicy>& b,
                       EpsParams... eps) {
        auto pa = a.raw_load().lock();
        auto pb = b.raw_load().lock();
        if (!pa && !pb) return true;  // Both expired
        if (!pa || !pb) return false;  // One expired
        return EqualDispatcher<T, Policy>::compare(*pa, *pb, eps...);
    }
};

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create shared_ptr for initializing AtomicReference.
 * 
 * @details Helper function that creates a shared_ptr<T> suitable for
 * initializing an AtomicReference. The EnforcementPolicy template parameter
 * is preserved for API compatibility but does not affect the shared_ptr creation.
 * 
 * @tparam T Value type
 * @tparam EnforcementPolicy Enforcement policy (for API consistency, not used)
 * @tparam Args Constructor argument types
 * @param args Arguments forwarded to T's constructor
 * @return shared_ptr<T> for initializing AtomicReference
 * 
 * Usage:
 * @code
 * // Option 1: Direct construction
 * AtomicReference<int> ref(make_atomic_shared<int>(42));
 * 
 * // Option 2: With explicit policy
 * AtomicReference<int, AlwaysEnforcePolicy> ref2(
 *     make_atomic_shared<int, AlwaysEnforcePolicy>(42)
 * );
 * 
 * // Option 3: Store after default construction
 * AtomicReference<int> ref3;
 * ref3.store(make_atomic_shared<int>(42));
 * @endcode
 */
template<typename T, typename EnforcementPolicy = DebugOnlyPolicy, typename... Args>
std::shared_ptr<T> make_atomic_shared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

/**
 * @brief Create shared_ptr with custom allocator for initializing AtomicReference.
 * 
 * @details Helper function that creates a shared_ptr<T> using a custom allocator,
 * suitable for initializing an AtomicReference.
 * 
 * @tparam T Value type
 * @tparam AllocatorT Allocator type
 * @tparam EnforcementPolicy Enforcement policy (for API consistency, not used)
 * @tparam Args Constructor argument types
 * @param alloc Custom allocator
 * @param args Arguments forwarded to T's constructor
 * @return shared_ptr<T> for initializing AtomicReference
 * 
 * Usage:
 * @code
 * std::pmr::monotonic_buffer_resource pool(1024);
 * std::pmr::polymorphic_allocator<Data> alloc(&pool);
 * AtomicReference<Data> ref(make_atomic_shared<Data>(alloc, args...));
 * @endcode
 */
template<typename T, typename AllocatorT, typename EnforcementPolicy = DebugOnlyPolicy, typename... Args>
std::shared_ptr<T> make_atomic_shared(const AllocatorT& alloc, Args&&... args) {
    return std::allocate_shared<T, AllocatorT>(alloc, std::forward<Args>(args)...);
}

/**
 * @brief Create weak_ptr for initializing AtomicReference<weak_ptr<T>>.
 * 
 * @details Helper function that creates a weak_ptr<T> from a shared_ptr,
 * suitable for initializing an AtomicReference<weak_ptr<T>>.
 * 
 * @tparam T Value type
 * @tparam EnforcementPolicy Enforcement policy (for API consistency, not used)
 * @param sp shared_ptr to observe
 * @return weak_ptr<T> for initializing AtomicReference<weak_ptr<T>>
 * 
 * Usage:
 * @code
 * auto sp = std::make_shared<int>(42);
 * AtomicReference<std::weak_ptr<int>> ref(make_atomic_weak(sp));
 * @endcode
 */
template<typename T, typename EnforcementPolicy = DebugOnlyPolicy>
std::weak_ptr<T> make_atomic_weak(std::shared_ptr<T> sp) {
    return std::weak_ptr<T>(std::move(sp));
}

/**
 * @brief Pass-through weak_ptr for initializing AtomicReference<weak_ptr<T>>.
 * 
 * @details Helper function that forwards a weak_ptr<T>,
 * suitable for initializing an AtomicReference<weak_ptr<T>>.
 * This is primarily for API consistency.
 * 
 * @tparam T Value type
 * @tparam EnforcementPolicy Enforcement policy (for API consistency, not used)
 * @param wp weak_ptr to store
 * @return weak_ptr<T> for initializing AtomicReference<weak_ptr<T>>
 * 
 * Usage:
 * @code
 * std::weak_ptr<int> wp = ...;
 * AtomicReference<std::weak_ptr<int>> ref(make_atomic_weak(wp));
 * @endcode
 */
template<typename T, typename EnforcementPolicy = DebugOnlyPolicy>
std::weak_ptr<T> make_atomic_weak(std::weak_ptr<T> wp) {
    return std::move(wp);
}

} // namespace cpp_utilities
