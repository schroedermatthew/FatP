# ConcurrencyPolicies.h Simplification Plan

**Document Version:** 2.0  
**Date:** 2026-02-03  
**Status:** Proposed  
**File:** `include/fat_p/ConcurrencyPolicies.h`  
**Lines:** 2,498  
**Focus:** Code simplification via C++20 concepts

---

## 1. Executive Summary

ConcurrencyPolicies.h contains **~170 lines of SFINAE boilerplate** that can be replaced with **~55 lines of C++20 concepts**. This is pure dead complexity that serves no purpose with C++20 minimum.

### Key Finding

```cpp
// CURRENT: 13 lines per trait × 13 traits = ~170 lines
template <typename T, typename = void>
struct is_shared_policy : std::false_type {};

template <typename T>
struct is_shared_policy<T, std::void_t<typename T::SharedGuard>> : std::true_type {};

template <typename T>
inline constexpr bool is_shared_policy_v = is_shared_policy<T>::value;

// SIMPLIFIED: 4 lines per trait × 13 traits = ~55 lines
template <typename T>
concept SharedPolicy = requires { typename T::SharedGuard; };

template <typename T>
inline constexpr bool is_shared_policy_v = SharedPolicy<T>;
```

### Impact

| Metric | Before | After | Reduction |
|--------|--------|-------|-----------|
| Trait section lines | ~170 | ~55 | **-115 lines** |
| SFINAE specializations | 26 | 0 | **-26** |
| `std::void_t` usages | 13 | 0 | **-13** |
| Compiler error quality | Poor | Excellent | — |

---

## 2. Current Traits Inventory

### 2.1 Simple Tag Traits (10 traits)

These check for the existence of a nested type:

| Trait | Checks For | Lines |
|-------|------------|-------|
| `is_concurrency_policy` | `T::PolicyTag` | 12 |
| `is_shared_policy` | `T::SharedGuard` | 12 |
| `is_fair_policy` | `T::FairOrderingTag` | 12 |
| `is_optimistic_policy` | `T::OptimisticTag` | 12 |
| `is_numa_aware_policy` | `T::NUMAAwareTag` | 12 |
| `is_realtime_policy` | `T::RealtimeTag` | 12 |
| `is_lockfree_policy` | `T::LockFreeTag` | 12 |
| `is_adaptive_policy` | `T::AdaptiveTag` | 12 |
| `is_recursive_policy` | `T::RecursiveTag` | 12 |

**Subtotal:** 108 lines → **36 lines** with concepts

### 2.2 Expression Traits (4 traits)

These check for valid expressions:

| Trait | Checks For | Lines |
|-------|------------|-------|
| `is_waitable_policy` | `LockGuard::wait(condition_variable&)` | 14 |
| `is_timed_policy` | `LockGuard::try_lock_for(duration)` | 14 |
| `has_contention_tracking` | `T::get_contention()` | 12 |
| `supports_try_lock` | `T::try_lock()` | 12 |

**Subtotal:** 52 lines → **20 lines** with concepts

### 2.3 Total

| Category | Before | After |
|----------|--------|-------|
| Tag traits (10) | 108 | 36 |
| Expression traits (4) | 52 | 20 |
| **Total** | **160** | **56** |

**Savings: 104 lines**

---

## 3. Simplified Implementation

### 3.1 Concept Definitions

```cpp
// =============================================================================
// Policy Concepts (C++20)
// =============================================================================

/// Policy has the basic concurrency policy interface
template <typename T>
concept ConcurrencyPolicyTag = requires { typename T::PolicyTag; };

/// Policy supports shared (read) locking
template <typename T>
concept SharedPolicy = requires { typename T::SharedGuard; };

/// Policy provides fair (FIFO) ordering
template <typename T>
concept FairPolicy = requires { typename T::FairOrderingTag; };

/// Policy uses optimistic concurrency (e.g., SeqLock)
template <typename T>
concept OptimisticPolicy = requires { typename T::OptimisticTag; };

/// Policy is NUMA-aware (e.g., MCSLock)
template <typename T>
concept NumaAwarePolicy = requires { typename T::NUMAAwareTag; };

/// Policy supports real-time priority inheritance
template <typename T>
concept RealtimePolicy = requires { typename T::RealtimeTag; };

/// Policy is lock-free
template <typename T>
concept LockFreePolicy = requires { typename T::LockFreeTag; };

/// Policy adapts between strategies at runtime
template <typename T>
concept AdaptivePolicy = requires { typename T::AdaptiveTag; };

/// Policy supports recursive locking
template <typename T>
concept RecursivePolicy = requires { typename T::RecursiveTag; };

/// Policy's LockGuard supports condition variable wait
template <typename T>
concept WaitablePolicy = requires(typename T::LockGuard g, std::condition_variable& cv) {
    g.wait(cv);
};

/// Policy's LockGuard supports timed locking
template <typename T>
concept TimedPolicy = requires(typename T::LockGuard g) {
    g.try_lock_for(std::chrono::milliseconds(1));
};

/// Policy tracks contention statistics
template <typename T>
concept HasContentionTracking = requires(T t) {
    { t.get_contention() } -> std::convertible_to<uint64_t>;
};

/// Policy supports try_lock()
template <typename T>
concept SupportsTryLock = requires(T t) {
    { t.try_lock() } -> std::convertible_to<bool>;
};
```

### 3.2 Backward Compatibility Variables

```cpp
// =============================================================================
// Backward Compatibility (variable templates)
// =============================================================================

template <typename T>
inline constexpr bool is_concurrency_policy_v = ConcurrencyPolicyTag<T>;

template <typename T>
inline constexpr bool is_shared_policy_v = SharedPolicy<T>;

template <typename T>
inline constexpr bool is_fair_policy_v = FairPolicy<T>;

template <typename T>
inline constexpr bool is_optimistic_policy_v = OptimisticPolicy<T>;

template <typename T>
inline constexpr bool is_numa_aware_policy_v = NumaAwarePolicy<T>;

template <typename T>
inline constexpr bool is_realtime_policy_v = RealtimePolicy<T>;

template <typename T>
inline constexpr bool is_lockfree_policy_v = LockFreePolicy<T>;

template <typename T>
inline constexpr bool is_adaptive_policy_v = AdaptivePolicy<T>;

template <typename T>
inline constexpr bool is_recursive_policy_v = RecursivePolicy<T>;

template <typename T>
inline constexpr bool is_waitable_policy_v = WaitablePolicy<T>;

template <typename T>
inline constexpr bool is_timed_policy_v = TimedPolicy<T>;

template <typename T>
inline constexpr bool has_contention_tracking_v = HasContentionTracking<T>;

template <typename T>
inline constexpr bool supports_try_lock_v = SupportsTryLock<T>;
```

### 3.3 Remove Old Struct Definitions

Delete lines 191-370 (the entire SFINAE trait section).

---

## 4. Additional Simplifications

### 4.1 jthread Detection (Lines 108-113)

**Current:**
```cpp
// C++23 jthread feature detection
#if FATP_HAS_CPP23 || FATP_HAS_JTHREAD
#define FATP_HAS_JTHREAD 1
#else
#define FATP_HAS_JTHREAD 0
#endif
```

**Issue:** 
1. Comment wrong (jthread is C++20)
2. Redundant — CppFeatureDetection.h already defines this

**Fix:** Delete entirely. CppFeatureDetection.h handles it.

**Savings:** 6 lines

### 4.2 Existing ConcurrencyPolicy Concept (Lines 2489-2496)

**Current:**
```cpp
template <typename P>
concept ConcurrencyPolicy = requires(P p)
{
    typename P::LockGuard;
    typename P::SharedGuard;
    {p.lock()}->std::same_as<typename P::LockGuard>;
    {p.lock_shared()}->std::same_as<typename P::SharedGuard>;
};
```

**Action:** Keep but move to the concepts section at the top.

---

## 5. Documentation Fixes

### 5.1 Line 44

**Before:**
```cpp
*   - Enhanced C++20/C++23 support with jthread and atomic<shared_ptr>
```

**After:**
```cpp
*   - C++20 jthread and atomic<shared_ptr> support (where library available)
```

### 5.2 Line 108

**Before:**
```cpp
// C++23 jthread feature detection
```

**After:** (deleted with the redundant detection block)

---

## 6. Side-by-Side Comparison

### Before (is_waitable_policy)

```cpp
template <typename T, typename = void>
struct is_waitable_policy : std::false_type
{
};

template <typename T>
struct is_waitable_policy<
    T,
    std::void_t<decltype(std::declval<typename T::LockGuard>().wait(std::declval<std::condition_variable&>()))>>
    : std::true_type
{
};

template <typename T>
inline constexpr bool is_waitable_policy_v = is_waitable_policy<T>::value;
```

**Lines: 14**

### After (WaitablePolicy)

```cpp
template <typename T>
concept WaitablePolicy = requires(typename T::LockGuard g, std::condition_variable& cv) {
    g.wait(cv);
};

template <typename T>
inline constexpr bool is_waitable_policy_v = WaitablePolicy<T>;
```

**Lines: 5**

**Reduction: 64%**

---

## 7. Benefits Beyond Line Count

### 7.1 Better Error Messages

**SFINAE failure:**
```
error: no type named 'type' in 'struct std::enable_if<false, void>'
```

**Concept failure:**
```
error: constraints not satisfied for 'WaitablePolicy<MyPolicy>'
note: the required expression 'g.wait(cv)' is invalid
```

### 7.2 Direct Use in Constraints

```cpp
// Before: verbose enable_if
template <typename Policy, std::enable_if_t<is_waitable_policy_v<Policy>, int> = 0>
void waitOnPolicy(Policy& p) { ... }

// After: clean requires clause
template <WaitablePolicy Policy>
void waitOnPolicy(Policy& p) { ... }

// Or inline constraint
void waitOnPolicy(WaitablePolicy auto& p) { ... }
```

### 7.3 Composability

```cpp
// Combine concepts naturally
template <typename T>
concept ThreadSafeWaitablePolicy = SharedPolicy<T> && WaitablePolicy<T>;
```

---

## 8. Implementation Plan

### Phase 1: Replace Traits with Concepts

1. Add concept definitions after includes (~45 lines)
2. Add backward-compat variables (~26 lines)
3. Delete old SFINAE structs (lines 191-370, ~180 lines)

**Net change:** -109 lines

### Phase 2: Clean Up Detection

1. Delete redundant jthread detection (lines 108-113)
2. Fix documentation comment (line 44)

**Net change:** -6 lines

### Phase 3: Move Existing Concept

1. Move `ConcurrencyPolicy` concept from line 2489 to concepts section
2. Ensure consistency with new naming

**Net change:** 0 lines (reorganization)

---

## 9. Verification

### 9.1 Compile Test

```cpp
// Verify concepts work correctly
static_assert(SharedPolicy<SharedMutexPolicy>);
static_assert(!SharedPolicy<int>);
static_assert(WaitablePolicy<WaitableSynchronizationPolicy>);
static_assert(FairPolicy<TicketLockPolicy>);
static_assert(NumaAwarePolicy<MCSLockPolicy>);

// Verify backward compat
static_assert(is_shared_policy_v<SharedMutexPolicy>);
static_assert(is_waitable_policy_v<WaitableSynchronizationPolicy>);
```

### 9.2 Existing Tests

Run `test_ConcurrencyPolicies.cpp` — all tests should pass unchanged since the `_v` variables are preserved.

---

## 10. Summary

### Total Changes

| Change | Lines |
|--------|-------|
| Add concepts section | +45 |
| Add backward-compat variables | +26 |
| Delete SFINAE structs | -180 |
| Delete jthread detection | -6 |
| Fix doc comment | 0 |
| **Net** | **-115 lines** |

### What Gets Simpler

| Before | After |
|--------|-------|
| 13 SFINAE trait structs (26 templates) | 13 concepts |
| `std::void_t` + `std::declval` patterns | Clean `requires` expressions |
| Poor error messages | Clear constraint failures |
| Redundant jthread detection | Rely on CppFeatureDetection.h |

### What Stays the Same

- All 19 policies (unchanged)
- FATP_USE_* configuration toggles (unchanged)
- Platform detection (unchanged)
- Windows header hygiene (unchanged)
- atomic<shared_ptr> fallback (unchanged — legitimate)
- All `_v` variable template names (backward compat)

### Conclusion

This is a **major simplification** — replacing 180 lines of SFINAE boilerplate with 71 lines of clean C++20 concepts while maintaining full backward compatibility via the `_v` variable templates.

**Effort:** 1-2 hours  
**Risk:** Low (tests verify correctness)  
**Benefit:** -115 lines, better errors, cleaner constraints

---

*End of Document*
