#pragma once

/*
FATP_META:
  meta_version: 1
  component: Signal
  file_role: public_header
  path: include/fat_p/Signal.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for Signal."
  api_stability: in_work
  related:
    docs_search: "Signal"
    tests:
      - components/Signal/tests/test_Signal.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file Signal.h
 * @brief High-performance Signal/Slot implementation for the fat_p library
 *
 *
 *
 * @details A cache-friendly, policy-based signal-slot system that integrates
 * with the fat_p ecosystem. Designed to solve three hard problems without
 * heap allocation for the common case (1-4 listeners):
 *
 * 1. Small Object Optimization: Uses SmallVector<Slot, 4> for stack storage
 * 2. Reentrancy Safety: Block-and-deferred-sweep algorithm for mid-emission disconnects
 * 3. Dangling Pointers: RAII ScopedConnection for automatic lifetime management
 *
 * Key Features:
 * - Zero heap allocation for signals with <= 4 listeners (configurable)
 * - Policy-based threading via ConcurrencyPolicies
 * - Type-safe connection IDs via StrongId
 * - RAII connection management via ScopeGuard
 * - Priority-based slot ordering
 * - Synchronous emission (async can be achieved by wrapping with ThreadPool)
 * - Exception-safe emission with configurable policies
 *
 * @section complexity Complexity
 * - connect: O(1) amortized (O(n) for priority insertion)
 * - disconnect: O(n) worst case, O(1) during emission (deferred)
 * - emit: O(n) where n = number of active slots
 * - memory: O(InlineCapacity) on stack, O(n) on heap when exceeded
 *
 * @section integration fat_p Integration Points
 * | Feature     | Component           | Benefit                                |
 * |-------------|---------------------|----------------------------------------|
 * | Storage     | SmallVector.h       | Zero-alloc for common case             |
 * | Locking     | ConcurrencyPolicies | Configurable thread safety             |
 * | Handles     | StrongId.h          | Type-safe connection IDs               |
 * | Safety      | ScopeGuard.h        | RAII connection lifetime               |
 * | Contracts   | enforce.h           | Debug precondition checking            |
 *
 * @version 1.0.0
 * @date 2025-11
 *
 * Requirements:
 * - C++17 or later
 * - fat_p headers: SmallVector.h, ConcurrencyPolicies.h, StrongId.h, ScopeGuard.h
 *
 * @code
 * // Basic usage
 * Signal<void(int)> onValueChanged;
 *
 * // Connect with automatic disconnection
 * auto conn = onValueChanged.connect([](int val) {
 *     std::cout << "Value: " << val << "\n";
 * });
 *
 * // Manual connection management
 * auto id = onValueChanged.connectManual([](int val) { ... });
 * onValueChanged.disconnect(id);
 *
 * // Emit to all listeners
 * onValueChanged.emit(42);
 *
 * // Thread-safe signal
 * Signal<void(int), SharedMutexPolicy> threadSafeSignal;
 *
 * // Priority ordering (higher = called first)
 * onValueChanged.connect([](int){}, 10);  // High priority
 * onValueChanged.connect([](int){}, -5);  // Low priority
 * @endcode
 */

#include <algorithm>
#include <atomic>
#include <functional>
#include <type_traits>
#include <utility>

#include "ConcurrencyPolicies.h"
#include "ScopeGuard.h"
#include "SmallVector.h"
#include "StrongId.h"

namespace fat_p
{

// =============================================================================
// Connection ID Type
// =============================================================================

/**
 * @brief Tag type for Signal connection IDs
 */
struct SignalConnectionTag
{
};

/**
 * @brief Type-safe connection identifier
 * @details Uses StrongId for compile-time type safety, preventing accidental
 * misuse of raw integers as connection handles.
 */
using ConnectionId = StrongId<size_t, SignalConnectionTag>;

/**
 * @brief Invalid connection ID sentinel value
 */
inline constexpr ConnectionId InvalidConnectionId{0};

// =============================================================================
// Emission Policy Tags
// =============================================================================

/**
 * @brief Policy that catches and ignores exceptions from slots during emission
 */
struct CatchAndIgnorePolicy
{
    template <typename F, typename... Args>
    static void invoke(F&& f, Args&&... args) noexcept
    {
        try
        {
            std::forward<F>(f)(std::forward<Args>(args)...);
        }
        catch (...)
        {
            // Swallow exception - slot failure doesn't affect others
        }
    }
};

/**
 * @brief Policy that allows exceptions to propagate from slots
 * @warning If a slot throws, subsequent slots won't be notified
 */
struct PropagateExceptionPolicy
{
    template <typename F, typename... Args>
    static void invoke(F&& f, Args&&... args)
    {
        std::forward<F>(f)(std::forward<Args>(args)...);
    }
};

/**
 * @brief Policy that terminates on slot exceptions (for noexcept emission)
 */
struct TerminateOnExceptionPolicy
{
    template <typename F, typename... Args>
    static void invoke(F&& f, Args&&... args) noexcept
    {
        std::forward<F>(f)(std::forward<Args>(args)...);
    }
};

// =============================================================================
// Forward Declarations
// =============================================================================

template <typename Signature,
          typename SyncPolicy = SingleThreadedPolicy,
          typename EmissionPolicy = CatchAndIgnorePolicy,
          size_t InlineCapacity = 4>
class Signal;

// =============================================================================
// ScopedConnection - RAII Connection Handle
// =============================================================================

/**
 * @brief RAII wrapper for automatic signal disconnection
 *
 * @details When a ScopedConnection goes out of scope, it automatically
 * disconnects from the signal. This prevents dangling callbacks and ensures
 * listeners don't outlive their intended scope.
 *
 * Move-only semantics ensure exactly one owner manages the connection lifetime.
 *
 * @code
 * class MyWidget {
 *     ScopedConnection mConnection;
 * public:
 *     void subscribe(Signal<void(int)>& signal) {
 *         mConnection = signal.connect([this](int v) { onValue(v); });
 *     }
 *     // Connection automatically disconnected when MyWidget is destroyed
 * };
 * @endcode
 */
class ScopedConnection
{
public:
    /**
     * @brief Default constructor - creates empty (unconnected) handle
     */
    ScopedConnection() noexcept = default;

    /**
     * @brief Construct from disconnect function
     * @param disconnector Function to call on destruction
     */
    explicit ScopedConnection(std::function<void()> disconnector) noexcept
        : mDisconnector(std::move(disconnector))
        , mConnected(true)
    {
    }

    /**
     * @brief Move constructor - transfers ownership
     */
    ScopedConnection(ScopedConnection&& other) noexcept
        : mDisconnector(std::move(other.mDisconnector))
        , mConnected(other.mConnected)
    {
        other.mConnected = false;
    }

    /**
     * @brief Move assignment - disconnects current, takes ownership
     */
    ScopedConnection& operator=(ScopedConnection&& other) noexcept
    {
        if (this != &other)
        {
            disconnect();
            mDisconnector = std::move(other.mDisconnector);
            mConnected = other.mConnected;
            other.mConnected = false;
        }
        return *this;
    }

    // Non-copyable
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

    /**
     * @brief Destructor - automatically disconnects if connected
     */
    ~ScopedConnection()
    {
        disconnect();
    }

    /**
     * @brief Manually disconnect (idempotent)
     */
    void disconnect() noexcept
    {
        if (mConnected && mDisconnector)
        {
            try
            {
                mDisconnector();
            }
            catch (...)
            {
                // Swallow exceptions in destructor context
            }
            mConnected = false;
        }
    }

    /**
     * @brief Release ownership without disconnecting
     * @details After release(), the connection remains active but this
     * ScopedConnection no longer manages it.
     */
    void release() noexcept
    {
        mConnected = false;
        mDisconnector = nullptr;
    }

    /**
     * @brief Check if this handle is connected
     */
    [[nodiscard]] bool isConnected() const noexcept
    {
        return mConnected;
    }

    /**
     * @brief Boolean conversion for connected state
     */
    explicit operator bool() const noexcept
    {
        return mConnected;
    }

private:
    std::function<void()> mDisconnector;
    bool mConnected = false;
};

// =============================================================================
// Signal Implementation
// =============================================================================

/**
 * @brief High-performance signal for observer pattern implementation
 *
 * @tparam R Return type (typically void)
 * @tparam Args Parameter types passed to slots
 * @tparam SyncPolicy Concurrency policy from ConcurrencyPolicies.h
 * @tparam EmissionPolicy How to handle slot exceptions during emit
 * @tparam InlineCapacity Number of slots to store inline (stack allocation)
 */
template <typename R, typename... Args, typename SyncPolicy, typename EmissionPolicy, size_t InlineCapacity>
class Signal<R(Args...), SyncPolicy, EmissionPolicy, InlineCapacity> : private SyncPolicy
{
public:
    // -------------------------------------------------------------------------
    // Type Aliases
    // -------------------------------------------------------------------------

    using Signature = R(Args...);
    using Callback = std::function<Signature>;
    using SyncPolicyType = SyncPolicy;
    using EmissionPolicyType = EmissionPolicy;

    static constexpr size_t inline_capacity = InlineCapacity;

private:
    // -------------------------------------------------------------------------
    // Internal Slot Structure
    // -------------------------------------------------------------------------

    struct Slot
    {
        ConnectionId id{InvalidConnectionId};
        Callback func;
        int priority = 0;
        // Atomic because disconnect() flips this under lock_shared() while emit()
        // reads it under lock_shared() on another thread. Cannot upgrade to write
        // lock because user callbacks may call disconnect() during emission, which
        // would deadlock on non-recursive shared_mutex.
        std::atomic<bool> active{true};

        Slot() = default;

        Slot(ConnectionId cid, Callback f, int prio)
            : id(cid)
            , func(std::move(f))
            , priority(prio)
            , active(true)
        {
        }

        // std::atomic is non-copyable/non-movable, so Slot needs explicit
        // move/copy to work in SmallVector. The active flag is always loaded
        // relaxed during structural operations (move/copy happen under write lock).
        Slot(const Slot& other)
            : id(other.id)
            , func(other.func)
            , priority(other.priority)
            , active(other.active.load(std::memory_order_relaxed))
        {
        }

        Slot(Slot&& other) noexcept
            : id(other.id)
            , func(std::move(other.func))
            , priority(other.priority)
            , active(other.active.load(std::memory_order_relaxed))
        {
        }

        Slot& operator=(const Slot& other)
        {
            if (this != &other)
            {
                id = other.id;
                func = other.func;
                priority = other.priority;
                active.store(other.active.load(std::memory_order_relaxed), std::memory_order_relaxed);
            }
            return *this;
        }

        Slot& operator=(Slot&& other) noexcept
        {
            if (this != &other)
            {
                id = other.id;
                func = std::move(other.func);
                priority = other.priority;
                active.store(other.active.load(std::memory_order_relaxed), std::memory_order_relaxed);
            }
            return *this;
        }

        // For priority-based sorting (higher priority = earlier in list)
        bool operator<(const Slot& other) const noexcept
        {
            return priority > other.priority; // Descending order
        }
    };

    // SmallVector storage: 95% of signals have <= 4 listeners
    using SlotList = SmallVector<Slot, InlineCapacity>;

    // -------------------------------------------------------------------------
    // Member Data
    // -------------------------------------------------------------------------

    SlotList mSlots;
    std::atomic<size_t> mNextId{1};         // Monotonic ID generator
    std::atomic<size_t> mRecursionDepth{0}; // Emission reentrancy counter
    std::atomic<bool> mNeedsCleanup{false}; // Deferred cleanup flag

public:
    // -------------------------------------------------------------------------
    // Constructors / Destructor
    // -------------------------------------------------------------------------

    Signal() = default;
    ~Signal() = default;

    // Non-copyable (signals represent unique event sources)
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;

    // Move semantics
    Signal(Signal&& other) noexcept
        : SyncPolicy() // Default construct base (policies shouldn't have state to move)
        , mSlots(std::move(other.mSlots))
        , mNextId(other.mNextId.load(std::memory_order_relaxed))
        , mRecursionDepth(0)
        , mNeedsCleanup(false)
    {
        other.mNextId.store(1, std::memory_order_relaxed);
    }

    Signal& operator=(Signal&& other) noexcept
    {
        if (this != &other)
        {
            [[maybe_unused]] auto lock = this->lock();
            mSlots = std::move(other.mSlots);
            mNextId.store(other.mNextId.load(std::memory_order_relaxed), std::memory_order_relaxed);
            mRecursionDepth.store(0, std::memory_order_relaxed);
            mNeedsCleanup.store(false, std::memory_order_relaxed);
            other.mNextId.store(1, std::memory_order_relaxed);
        }
        return *this;
    }

    // -------------------------------------------------------------------------
    // Connection API
    // -------------------------------------------------------------------------

    /**
     * @brief Connect a callback with automatic RAII disconnection
     *
     * @param callback The function to call when signal is emitted
     * @param priority Higher priority slots are called first (default: 0)
     * @return ScopedConnection that disconnects when destroyed
     *
     * @note Thread-safe when using appropriate SyncPolicy
     */
    [[nodiscard]] ScopedConnection connect(Callback callback, int priority = 0)
    {
        ConnectionId id = connectManual(std::move(callback), priority);

        // Capture weak reference to handle signal destruction
        return ScopedConnection([this, id]() {
            this->disconnect(id);
        });
    }

    /**
     * @brief Connect a callback with manual lifetime management
     *
     * @param callback The function to call when signal is emitted
     * @param priority Higher priority slots are called first (default: 0)
     * @return ConnectionId for manual disconnection
     *
     * @warning Caller is responsible for calling disconnect() before
     * callback becomes invalid (e.g., captured 'this' pointer destroyed)
     */
    ConnectionId connectManual(Callback callback, int priority = 0)
    {
        [[maybe_unused]] auto lock = this->lock();

        ConnectionId id(mNextId.fetch_add(1, std::memory_order_relaxed));

        // Find insertion point to maintain priority order
        // Use upper_bound so equal priorities maintain insertion order (FIFO)
        auto it = std::upper_bound(mSlots.begin(), mSlots.end(), priority, [](int prio, const Slot& slot) {
            return prio > slot.priority;
        });

        mSlots.insert(it, Slot{id, std::move(callback), priority});

        return id;
    }

    /**
     * @brief Connect a member function with automatic disconnection
     *
     * @tparam T Object type
     * @param obj Pointer to the object
     * @param method Member function pointer
     * @param priority Higher priority slots are called first (default: 0)
     * @return ScopedConnection that disconnects when destroyed
     *
     * @code
     * class Handler {
     * public:
     *     void onEvent(int value) { ... }
     * };
     * Handler h;
     * auto conn = signal.connect(&h, &Handler::onEvent);
     * @endcode
     */
    template <typename T>
    [[nodiscard]] ScopedConnection connect(T* obj, R (T::*method)(Args...), int priority = 0)
    {
        return connect(
            [obj, method](Args... args) {
                (obj->*method)(std::forward<Args>(args)...);
            },
            priority);
    }

    /**
     * @brief Disconnect a slot by its connection ID
     *
     * @param id The ConnectionId returned from connectManual()
     * @return true if the slot was found and disconnected
     *
     * @note If called during emission, performs soft-delete for reentrancy safety
     * @note Thread-safe: can be called from within slot callbacks without deadlock
     */
    bool disconnect(ConnectionId id)
    {
        // Check if we're currently emitting (from any thread)
        // During emission, we only need to soft-delete (set active=false)
        // which can be done with a read lock since it's just a flag flip
        if (mRecursionDepth.load(std::memory_order_acquire) > 0)
        {
            // We might be inside emit() on this or another thread
            // Use read lock for soft delete to avoid deadlock
            [[maybe_unused]] auto lock = this->lock_shared();

            for (auto& slot : mSlots)
            {
                if (slot.id == id && slot.active.load(std::memory_order_relaxed))
                {
                    slot.active.store(false, std::memory_order_relaxed); // Soft delete
                    mNeedsCleanup.store(true, std::memory_order_release);
                    return true;
                }
            }
            return false;
        }

        // Not emitting: acquire write lock for immediate removal
        [[maybe_unused]] auto lock = this->lock();

        // Re-check recursion depth under write lock
        if (mRecursionDepth.load(std::memory_order_acquire) > 0)
        {
            // Race: emission started between our checks
            for (auto& slot : mSlots)
            {
                if (slot.id == id && slot.active.load(std::memory_order_relaxed))
                {
                    slot.active.store(false, std::memory_order_relaxed);
                    mNeedsCleanup.store(true, std::memory_order_release);
                    return true;
                }
            }
            return false;
        }

        // Safe to physically remove
        for (auto it = mSlots.begin(); it != mSlots.end(); ++it)
        {
            if (it->id == id)
            {
                mSlots.erase(it);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Disconnect all slots
     */
    void disconnectAll()
    {
        if (mRecursionDepth.load(std::memory_order_acquire) > 0)
        {
            // During emission: soft delete with read lock
            [[maybe_unused]] auto lock = this->lock_shared();
            for (auto& slot : mSlots)
            {
                slot.active.store(false, std::memory_order_relaxed);
            }
            mNeedsCleanup.store(true, std::memory_order_release);
            return;
        }

        [[maybe_unused]] auto lock = this->lock();

        // Re-check under write lock
        if (mRecursionDepth.load(std::memory_order_acquire) > 0)
        {
            for (auto& slot : mSlots)
            {
                slot.active.store(false, std::memory_order_relaxed);
            }
            mNeedsCleanup.store(true, std::memory_order_release);
        }
        else
        {
            mSlots.clear();
        }
    }

    // -------------------------------------------------------------------------
    // Emission API
    // -------------------------------------------------------------------------

    /**
     * @brief Emit signal to all connected slots
     *
     * @param args Arguments forwarded to all slot callbacks
     *
     * @note Reentrancy-safe: slots can connect/disconnect during emission
     * @note Exception handling controlled by EmissionPolicy
     */
    void emit(Args... args)
    {
        bool shouldCleanup = false;

        {
            [[maybe_unused]] auto lock = this->lock_shared();

            // Enter emission context
            mRecursionDepth.fetch_add(1, std::memory_order_acquire);

            // RAII guard for decrement on exit (exception-safe)
            auto depthGuard = makeScopeGuard([this, &shouldCleanup]() noexcept {
                size_t depth = mRecursionDepth.fetch_sub(1, std::memory_order_release);

                // Mark for cleanup if we're the outermost emission and cleanup is needed
                if (depth == 1 && mNeedsCleanup.load(std::memory_order_acquire))
                {
                    shouldCleanup = true;
                }
            });

            // Invoke all active slots
            for (auto& slot : mSlots)
            {
                if (slot.active.load(std::memory_order_relaxed))
                {
                    EmissionPolicy::invoke(slot.func, args...);
                }
            }
        }
        // SharedGuard released here

        // Perform cleanup outside the read lock to avoid deadlock
        if (shouldCleanup)
        {
            performDeferredCleanup();
        }
    }

    /**
     * @brief Call operator for convenient emission syntax
     *
     * @code
     * Signal<void(int)> sig;
     * sig(42);  // Same as sig.emit(42)
     * @endcode
     */
    void operator()(Args... args)
    {
        emit(std::forward<Args>(args)...);
    }

    // -------------------------------------------------------------------------
    // Query API
    // -------------------------------------------------------------------------

    /**
     * @brief Get number of connected slots (including inactive pending cleanup)
     */
    [[nodiscard]] size_t slotCount() const
    {
        [[maybe_unused]] auto lock = this->lock_shared();
        return mSlots.size();
    }

    /**
     * @brief Get number of active (non-tombstone) slots
     */
    [[nodiscard]] size_t activeSlotCount() const
    {
        [[maybe_unused]] auto lock = this->lock_shared();
        return static_cast<size_t>(std::count_if(mSlots.begin(), mSlots.end(), [](const Slot& s) {
            return s.active.load(std::memory_order_relaxed);
        }));
    }

    /**
     * @brief Check if any slots are connected
     */
    [[nodiscard]] bool hasConnections() const
    {
        return activeSlotCount() > 0;
    }

    /**
     * @brief Check if currently emitting (for debugging/diagnostics)
     */
    [[nodiscard]] bool isEmitting() const noexcept
    {
        return mRecursionDepth.load(std::memory_order_acquire) > 0;
    }

    /**
     * @brief Check if a specific connection is still active
     */
    [[nodiscard]] bool isConnected(ConnectionId id) const
    {
        [[maybe_unused]] auto lock = this->lock_shared();

        for (const auto& slot : mSlots)
        {
            if (slot.id == id)
            {
                return slot.active.load(std::memory_order_relaxed);
            }
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // Blocking API (for slots with return values)
    // -------------------------------------------------------------------------

    /**
     * @brief Emit and collect results from all slots
     *
     * @tparam Container Container type to store results (e.g., std::vector<R>)
     * @param args Arguments forwarded to all slot callbacks
     * @return Container with results from each slot
     *
     * @note Only available when R is not void
     */
    template <typename Ret = R,
              typename Container = SmallVector<Ret, InlineCapacity>>
        requires (!std::is_void_v<Ret>)
    [[nodiscard]] Container emitCollect(Args... args)
    {
        Container results;
        bool shouldCleanup = false;

        {
            [[maybe_unused]] auto lock = this->lock_shared();

            results.reserve(mSlots.size());

            mRecursionDepth.fetch_add(1, std::memory_order_acquire);

            auto depthGuard = makeScopeGuard([this, &shouldCleanup]() noexcept {
                size_t depth = mRecursionDepth.fetch_sub(1, std::memory_order_release);
                if (depth == 1 && mNeedsCleanup.load(std::memory_order_acquire))
                {
                    shouldCleanup = true;
                }
            });

            for (auto& slot : mSlots)
            {
                if (slot.active.load(std::memory_order_relaxed))
                {
                    try
                    {
                        results.push_back(slot.func(args...));
                    }
                    catch (...)
                    {
                        if constexpr (std::is_same_v<EmissionPolicy, PropagateExceptionPolicy>)
                        {
                            throw;
                        }
                        // CatchAndIgnore: skip this slot's result
                    }
                }
            }
        }

        if (shouldCleanup)
        {
            performDeferredCleanup();
        }

        return results;
    }

    /**
     * @brief Emit until a slot returns a truthy value (short-circuit)
     *
     * @param args Arguments forwarded to slot callbacks
     * @return The first truthy result, or default R{} if none
     *
     * @note Only available when R is convertible to bool
     */
    template <typename Ret = R>
        requires (!std::is_void_v<Ret> && std::is_convertible_v<Ret, bool>)
    [[nodiscard]] Ret emitUntil(Args... args)
    {
        Ret result{};
        bool shouldCleanup = false;

        {
            [[maybe_unused]] auto lock = this->lock_shared();

            mRecursionDepth.fetch_add(1, std::memory_order_acquire);

            auto depthGuard = makeScopeGuard([this, &shouldCleanup]() noexcept {
                size_t depth = mRecursionDepth.fetch_sub(1, std::memory_order_release);
                if (depth == 1 && mNeedsCleanup.load(std::memory_order_acquire))
                {
                    shouldCleanup = true;
                }
            });

            for (auto& slot : mSlots)
            {
                if (slot.active.load(std::memory_order_relaxed))
                {
                    try
                    {
                        Ret r = slot.func(args...);
                        if (static_cast<bool>(r))
                        {
                            result = std::move(r);
                            break;
                        }
                    }
                    catch (...)
                    {
                        if constexpr (std::is_same_v<EmissionPolicy, PropagateExceptionPolicy>)
                        {
                            throw;
                        }
                    }
                }
            }
        }

        if (shouldCleanup)
        {
            performDeferredCleanup();
        }

        return result;
    }

private:
    // -------------------------------------------------------------------------
    // Internal Helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Remove tombstoned slots after emission completes
     *
     * Uses erase-remove idiom to compact the SmallVector, removing
     * slots that were soft-deleted during emission.
     */
    void performDeferredCleanup()
    {
        [[maybe_unused]] auto lock = this->lock();

        // Only cleanup if no longer emitting
        if (mRecursionDepth.load(std::memory_order_acquire) == 0)
        {
            mSlots.erase(std::remove_if(mSlots.begin(),
                                        mSlots.end(),
                                        [](const Slot& s) {
                                            return !s.active.load(std::memory_order_relaxed);
                                        }),
                         mSlots.end());
            mNeedsCleanup.store(false, std::memory_order_release);
        }
    }
};

// =============================================================================
// Type Trait Specialization
// =============================================================================

/**
 * @brief Type trait to detect Signal types
 */
template <typename T>
struct is_signal : std::false_type
{
};

template <typename Sig, typename Sync, typename Emit, size_t N>
struct is_signal<Signal<Sig, Sync, Emit, N>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_signal_v = is_signal<T>::value;

// =============================================================================
// Convenience Type Aliases
// =============================================================================

/**
 * @brief Thread-safe signal using SharedMutexPolicy
 */
template <typename Signature, size_t InlineCapacity = 4>
using ThreadSafeSignal = Signal<Signature, SharedMutexPolicy, CatchAndIgnorePolicy, InlineCapacity>;

/**
 * @brief Spinlock-based signal for low-latency scenarios
 */
template <typename Signature, size_t InlineCapacity = 4>
using SpinlockSignal = Signal<Signature, SpinlockSynchronizationPolicy, CatchAndIgnorePolicy, InlineCapacity>;

/**
 * @brief Single-threaded signal (zero synchronization overhead)
 */
template <typename Signature, size_t InlineCapacity = 4>
using LocalSignal = Signal<Signature, SingleThreadedPolicy, CatchAndIgnorePolicy, InlineCapacity>;

} // namespace fat_p
