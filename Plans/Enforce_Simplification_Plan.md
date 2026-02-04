# enforce.h Simplification Plan

**Document Version:** 1.0  
**Date:** 2026-02-03  
**Status:** Proposed  
**Files:** `enforce.h`, `enforce_enforcers.h`  
**Lines:** 879 + 321 = 1,200  
**Focus:** Replace FATP_LOCUS macro with C++20 `std::source_location`

---

## 1. Executive Summary

The enforce subsystem uses a custom `FATP_LOCUS` macro pattern (64 usages) to capture file/line information. C++20's `std::source_location` provides this automatically as a default parameter, eliminating the need for the macro machinery and simplifying every call site.

### Key Finding

```cpp
// CURRENT: Macro-based locus passing (3 macros, 64 usages)
#define FATP_LOCUS __FILE__ ":" FATP_STRINGIFY(__LINE__)
#define FATP_STRINGIFY(x) FATP_TOSTRING(x)
#define FATP_TOSTRING(x) #x

template <typename Policy>
auto enforce_policy_impl(bool passed, const char* expression_str, const char* locus);

// Called via:
enforce_policy_impl<Policy>(cond, "cond", FATP_LOCUS);

// SIMPLIFIED: std::source_location as default parameter
template <typename Policy>
auto enforce_policy_impl(bool passed, 
                         const char* expression_str,
                         std::source_location loc = std::source_location::current());

// Called via:
enforce_policy_impl<Policy>(cond, "cond");  // loc captured automatically!
```

### Impact

| Metric | Before | After | Reduction |
|--------|--------|-------|-----------|
| Locus-related macros | 3 | 0 | **-3 macros** |
| `FATP_LOCUS` call sites | 64 | 0 | **-64 usages** |
| Function parameters | 3 (`passed`, `expr`, `locus`) | 3 (but cleaner) | — |
| Info available | file:line | file, line, column, function | **Better** |

---

## 2. Current Pattern Analysis

### 2.1 FATP_LOCUS Macro (Lines 57-61)

```cpp
#ifndef FATP_LOCUS
#define FATP_LOCUS __FILE__ ":" FATP_STRINGIFY(__LINE__)
#define FATP_STRINGIFY(x) FATP_TOSTRING(x)
#define FATP_TOSTRING(x) #x
#endif
```

**Problems:**
1. Requires two helper macros for stringification
2. Every call site must remember to pass `FATP_LOCUS`
3. Only provides file and line (no function name, no column)
4. String concatenation at compile time is awkward

### 2.2 Current Function Signatures

```cpp
// enforce.h line 82
template <typename Policy>
[[nodiscard]] auto enforce_policy_impl(bool passed, 
                                       const char* expression_str, 
                                       const char* locus)

// enforce.h lines 95-99
template <typename... Msgs>
inline void debug_enforce_impl([[maybe_unused]] bool condition,
                               [[maybe_unused]] const char* expression_str,
                               [[maybe_unused]] const char* locus,
                               [[maybe_unused]] Msgs&&... msgs)
```

### 2.3 Current Macro Usage

```cpp
// Line 189
#define FATP_ENFORCE(condition, ...) \
    fat_p::debug_enforce_impl((condition), #condition, FATP_LOCUS, ##__VA_ARGS__)

// Line 805
#define FATP_DEBUG_ENFORCE_NOT_NULL(ptr, ...) \
    fat_p::debug_enforce_predicate_1<fat_p::NotNullPredicate>( \
        ptr, "not_null(" #ptr ")", FATP_LOCUS, ##__VA_ARGS__)
```

---

## 3. C++20 std::source_location Solution

### 3.1 New Function Signatures

```cpp
#include <source_location>

template <typename Policy>
[[nodiscard]] auto enforce_policy_impl(
    bool passed, 
    const char* expression_str,
    std::source_location loc = std::source_location::current())
{
    using Raiser = typename RaiserSelector<Policy>::type;
    return MakeEnforcer<Raiser>(passed, expression_str, loc);
}

template <typename... Msgs>
inline void debug_enforce_impl(
    [[maybe_unused]] bool condition,
    [[maybe_unused]] const char* expression_str,
    [[maybe_unused]] Msgs&&... msgs,
    [[maybe_unused]] std::source_location loc = std::source_location::current())
{
    if constexpr (FATP_DEBUG_ENFORCE_ENABLED)
    {
        auto enforcer = enforce_policy_impl<DebugOnlyPolicy>(condition, expression_str, loc);
        enforcer(std::forward<Msgs>(msgs)...);
    }
}
```

**Note:** The `loc` parameter must come after the variadic pack OR we need a different approach for variadic functions.

### 3.2 Variadic Function Challenge

The current signatures have `Msgs&&... msgs` before the location. With `source_location`, the default parameter must come last, but you can't have parameters after a pack.

**Solution:** Move `source_location` before the pack, make it non-default in the function, but have macros not pass it:

```cpp
// Option A: source_location before pack (requires macro change)
template <typename... Msgs>
inline void debug_enforce_impl(
    bool condition,
    const char* expression_str,
    std::source_location loc,  // No default - macro handles it
    Msgs&&... msgs)

// Macro captures location:
#define FATP_ENFORCE(condition, ...) \
    fat_p::debug_enforce_impl((condition), #condition, \
        std::source_location::current(), ##__VA_ARGS__)
```

```cpp
// Option B: Wrapper struct (cleaner, no macro change needed)
struct LocCapture {
    std::source_location loc;
    LocCapture(std::source_location l = std::source_location::current()) : loc(l) {}
};

template <typename... Msgs>
inline void debug_enforce_impl(
    bool condition,
    const char* expression_str,
    Msgs&&... msgs,
    LocCapture loc = {})  // Works! Captures call site
```

**Recommended: Option B** — The wrapper struct trick allows `source_location` to work as the last parameter even with variadic packs.

### 3.3 Simplified Macros

```cpp
// BEFORE
#define FATP_ENFORCE(condition, ...) \
    fat_p::debug_enforce_impl((condition), #condition, FATP_LOCUS, ##__VA_ARGS__)

// AFTER  
#define FATP_ENFORCE(condition, ...) \
    fat_p::debug_enforce_impl((condition), #condition, ##__VA_ARGS__)
```

The `FATP_LOCUS` argument is simply removed from all 64 macro invocations.

---

## 4. Enforcer Class Update

### 4.1 Current (enforce_enforcers.h lines 157-161)

```cpp
constexpr Enforcer(bool passed, const char* expression_str, const char* locus) noexcept
    : mPassed(passed)
    , mLocus(locus)
    , mExpression(expression_str)
{
}
```

### 4.2 Simplified

```cpp
constexpr Enforcer(bool passed, 
                   const char* expression_str, 
                   std::source_location loc) noexcept
    : mPassed(passed)
    , mLoc(loc)
    , mExpression(expression_str)
{
}
```

### 4.3 Better Error Messages

```cpp
// BEFORE: "Condition: x > 0\n\tLocus: main.cpp:42"

// AFTER: More information available
void fail_impl()
{
    std::string full_message = "\n\tCondition: ";
    full_message += mExpression;
    full_message += "\n\tFile: ";
    full_message += mLoc.file_name();
    full_message += "\n\tLine: ";
    full_message += std::to_string(mLoc.line());
    full_message += "\n\tFunction: ";
    full_message += mLoc.function_name();
    // ...
}
```

---

## 5. Implementation Summary

### 5.1 Changes to enforce.h

| Change | Lines Affected |
|--------|----------------|
| Remove FATP_LOCUS macros | -5 lines (57-61) |
| Add `#include <source_location>` | +1 line |
| Add LocCapture wrapper | +4 lines |
| Update `enforce_policy_impl` signature | ~2 lines changed |
| Update 6 `debug_enforce_*` functions | ~12 lines changed |
| Remove FATP_LOCUS from ~50 macros | ~50 changes (shorter lines) |

### 5.2 Changes to enforce_enforcers.h

| Change | Lines Affected |
|--------|----------------|
| Add `#include <source_location>` | +1 line |
| Change `const char* locus` to `std::source_location` | ~5 lines |
| Update `fail_impl()` to use source_location | ~8 lines changed |
| Update `MakeEnforcer` signature | ~2 lines |

### 5.3 Net Impact

| Metric | Change |
|--------|--------|
| Macros removed | 3 (FATP_LOCUS, FATP_STRINGIFY, FATP_TOSTRING) |
| Lines removed (FATP_LOCUS in calls) | ~64 shorter lines |
| New includes | 1 (`<source_location>`) |
| New helper | 1 (LocCapture, 4 lines) |
| **Net lines** | **~-10 to -15** |

---

## 6. Additional Benefits

### 6.1 Function Name in Errors

```
// BEFORE
Contract violation:
    Condition: ptr != nullptr
    Locus: MyClass.cpp:142
    Message: Invalid pointer

// AFTER
Contract violation:
    Condition: ptr != nullptr
    File: MyClass.cpp
    Line: 142
    Function: void MyClass::process(Data*)
    Message: Invalid pointer
```

### 6.2 No More Macro Pitfalls

The FATP_LOCUS pattern can fail silently if someone forgets to pass it:

```cpp
// Easy mistake - compiles but wrong location
enforce_policy_impl<Policy>(cond, "cond", "unknown");  // Oops!

// With source_location - automatically correct
enforce_policy_impl<Policy>(cond, "cond");  // Always right
```

### 6.3 Debugger Integration

`std::source_location` is recognized by debuggers and IDE tooling better than string concatenation macros.

---

## 7. Backward Compatibility

### 7.1 FATP_LOCUS Guard

The current code has:
```cpp
#ifndef FATP_LOCUS
#define FATP_LOCUS ...
#endif
```

This suggests users might define their own FATP_LOCUS. For backward compatibility, we could keep a deprecated version:

```cpp
// Deprecated - for backward compatibility only
#ifdef FATP_LOCUS
#warning "FATP_LOCUS is deprecated - enforce now uses std::source_location"
#endif
```

### 7.2 ABI Consideration

Changing function signatures from `const char*` to `std::source_location` is an ABI break. Since these are template functions and everything is header-only, this is not a concern.

---

## 8. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Variadic + source_location issue | Medium | Medium | LocCapture wrapper solves it |
| Performance overhead | Very Low | Low | source_location is constexpr-friendly |
| Missing include | Low | Low | Compiler error is clear |

---

## 9. Summary

### What Changes

| Before | After |
|--------|-------|
| 3 FATP_LOCUS macros | 0 (use std::source_location) |
| `const char* locus` parameters | `std::source_location loc` |
| Manual FATP_LOCUS at 64 call sites | Automatic capture |
| file:line only | file, line, column, function |

### What Stays the Same

- All 90 enforcement macros (still needed for expression stringification)
- FATP_DEBUG_ENFORCE_ENABLED pattern
- Policy-based design
- Zero-cost debug enforcement guarantee

### Conclusion

This is a **clean simplification** that:
1. Removes 3 helper macros
2. Eliminates 64 manual `FATP_LOCUS` arguments  
3. Provides better error information (function name!)
4. Makes it impossible to pass wrong location

**Effort:** 1-2 hours  
**Risk:** Low  
**Benefit:** Cleaner code, better errors, fewer macros

---

*End of Document*
