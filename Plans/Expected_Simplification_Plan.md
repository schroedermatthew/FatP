# Expected.h Simplification Plan

**Document Version:** 2.0  
**Date:** 2026-02-03  
**Status:** Proposed  
**File:** `include/fat_p/Expected.h`  
**Lines:** 4,098  
**Focus:** Code simplification via C++20 concepts and requires clauses

---

## 1. Executive Summary

Expected.h contains **21 `std::enable_if` patterns** that can be replaced with **clean C++20 `requires` clauses**. Several of these are 10+ line monstrosities that become 2-3 line constraints. Additionally, 4 SFINAE trait structs can become concepts.

### Key Finding

```cpp
// CURRENT: 10-line enable_if monstrosity
template <typename U,
          typename G,
          template <typename, typename> class SP,
          typename = std::enable_if_t<std::is_constructible_v<T, const U&> && 
                                      std::is_constructible_v<E, const G&> &&
                                      !std::is_constructible_v<T, ExpectedImpl<U, G, SP>&> &&
                                      !std::is_constructible_v<T, const ExpectedImpl<U, G, SP>&> &&
                                      !std::is_constructible_v<T, ExpectedImpl<U, G, SP>&&> &&
                                      !std::is_constructible_v<T, const ExpectedImpl<U, G, SP>&&> &&
                                      !std::is_convertible_v<ExpectedImpl<U, G, SP>&, T> &&
                                      !std::is_convertible_v<const ExpectedImpl<U, G, SP>&, T> &&
                                      !std::is_convertible_v<ExpectedImpl<U, G, SP>&&, T> &&
                                      !std::is_convertible_v<const ExpectedImpl<U, G, SP>&&, T>>>
explicit ExpectedImpl(const ExpectedImpl<U, G, SP>& other);

// SIMPLIFIED: Clean requires clause
template <typename U, typename G, template <typename, typename> class SP>
    requires std::constructible_from<T, const U&> && 
             std::constructible_from<E, const G&> &&
             (!std::constructible_from<T, ExpectedImpl<U, G, SP>> &&
              !std::convertible_to<ExpectedImpl<U, G, SP>, T>)
explicit ExpectedImpl(const ExpectedImpl<U, G, SP>& other);
```

### Impact

| Metric | Before | After | Reduction |
|--------|--------|-------|-----------|
| `enable_if` usages | 21 | 0 | **-21** |
| Trait struct lines | ~35 | ~12 | **-23 lines** |
| Complex constraints | 10+ lines each | 3-4 lines | **~60% shorter** |
| Error message quality | Poor | Excellent | — |

---

## 2. SFINAE Trait Simplifications

### 2.1 Current Traits (Lines 1027-1055)

```cpp
// is_expected_compatible - 6 lines
template <typename U, typename Err>
struct is_expected_compatible : std::false_type {};

template <typename V, typename Err, template <typename, typename> class SP>
struct is_expected_compatible<ExpectedImpl<V, Err, SP>, Err> : std::true_type {};

// is_expected_with_value - 6 lines
template <typename U, typename Val>
struct is_expected_with_value : std::false_type {};

template <typename Val, typename Err, template <typename, typename> class SP>
struct is_expected_with_value<ExpectedImpl<Val, Err, SP>, Val> : std::true_type {};

// is_expected - 4 lines (+ specialization at line 3966)
template <typename T>
struct is_expected : std::false_type {};

// is_expected_like - 4 lines (+ specializations)
template <typename T>
struct is_expected_like : std::false_type {};
```

### 2.2 Simplified with Concepts

```cpp
// =============================================================================
// Expected Type Traits (C++20 Concepts)
// =============================================================================

namespace detail {

/// Check if T is an ExpectedImpl instantiation
template <typename T>
struct is_expected_impl : std::false_type {};

template <typename V, typename E, template <typename, typename> class SP>
struct is_expected_impl<ExpectedImpl<V, E, SP>> : std::true_type {};

} // namespace detail

/// Type is an ExpectedImpl
template <typename T>
concept Expected = detail::is_expected_impl<std::remove_cvref_t<T>>::value;

/// Type is Expected-like (ExpectedImpl or std::expected)
template <typename T>
concept ExpectedLike = Expected<T> || requires {
    requires requires(T t) {
        { t.has_value() } -> std::convertible_to<bool>;
        t.value();
        t.error();
    };
};

/// Expected has compatible error type
template <typename T, typename Err>
concept ExpectedCompatible = Expected<T> && 
    std::same_as<typename std::remove_cvref_t<T>::error_type, Err>;

/// Expected has compatible value type  
template <typename T, typename Val>
concept ExpectedWithValue = Expected<T> && 
    std::same_as<typename std::remove_cvref_t<T>::value_type, Val>;

// Backward compatibility
template <typename T>
inline constexpr bool is_expected_v = Expected<T>;

template <typename T>
inline constexpr bool is_expected_like_v = ExpectedLike<T>;

template <typename T, typename Err>
inline constexpr bool is_expected_compatible_v = ExpectedCompatible<T, Err>;

template <typename T, typename Val>
inline constexpr bool is_expected_with_value_v = ExpectedWithValue<T, Val>;
```

---

## 3. Constructor Constraint Simplifications

### 3.1 Default Constructor (Line 1142)

**Before:**
```cpp
template <typename Dummy = void, typename = std::enable_if_t<std::is_default_constructible_v<T>, Dummy>>
constexpr ExpectedImpl() noexcept(std::is_nothrow_default_constructible_v<T>)
```

**After:**
```cpp
constexpr ExpectedImpl() noexcept(std::is_nothrow_default_constructible_v<T>)
    requires std::default_initializable<T>
```

### 3.2 Value Copy Constructor (Lines 1158-1161)

**Before:**
```cpp
template <typename U = T,
          typename = std::enable_if_t<std::is_constructible_v<T, const U&> &&
                                      !std::is_same_v<std::decay_t<U>, std::in_place_t> &&
                                      !std::is_same_v<std::decay_t<U>, ExpectedImpl>>>
constexpr ExpectedImpl(const U& v)
```

**After:**
```cpp
template <typename U = T>
    requires std::constructible_from<T, const U&> &&
             (!std::same_as<std::remove_cvref_t<U>, std::in_place_t>) &&
             (!std::same_as<std::remove_cvref_t<U>, ExpectedImpl>)
constexpr ExpectedImpl(const U& v)
```

### 3.3 Value Move Constructor (Lines 1171-1173)

**Before:**
```cpp
template <typename U = T,
          typename = std::enable_if_t<std::is_constructible_v<T, U&&> &&
                                      !std::is_same_v<std::decay_t<U>, std::in_place_t> &&
                                      !std::is_same_v<std::decay_t<U>, ExpectedImpl>>>
constexpr ExpectedImpl(U&& v)
```

**After:**
```cpp
template <typename U = T>
    requires std::constructible_from<T, U&&> &&
             (!std::same_as<std::remove_cvref_t<U>, std::in_place_t>) &&
             (!std::same_as<std::remove_cvref_t<U>, ExpectedImpl>)
constexpr ExpectedImpl(U&& v)
```

### 3.4 In-place Value Constructor (Line 1183)

**Before:**
```cpp
template <typename... Args, typename = std::enable_if_t<std::is_constructible_v<T, Args...>>>
explicit constexpr ExpectedImpl(std::in_place_t, Args&&... args)
```

**After:**
```cpp
template <typename... Args>
    requires std::constructible_from<T, Args...>
explicit constexpr ExpectedImpl(std::in_place_t, Args&&... args)
```

### 3.5 In-place Error Constructor (Line 1208)

**Before:**
```cpp
template <typename... Args, typename = std::enable_if_t<std::is_constructible_v<E, Args...>>>
explicit constexpr ExpectedImpl(unexpect_tag_t, Args&&... args)
```

**After:**
```cpp
template <typename... Args>
    requires std::constructible_from<E, Args...>
explicit constexpr ExpectedImpl(unexpect_tag_t, Args&&... args)
```

### 3.6 Converting Copy Constructor (Lines 1294-1302) — THE BIG ONE

**Before (10-line constraint!):**
```cpp
template <
    typename U,
    typename G,
    template <typename, typename> class SP,
    typename = std::enable_if_t<std::is_constructible_v<T, const U&> && std::is_constructible_v<E, const G&> &&
                                !std::is_constructible_v<T, ExpectedImpl<U, G, SP>&> &&
                                !std::is_constructible_v<T, const ExpectedImpl<U, G, SP>&> &&
                                !std::is_constructible_v<T, ExpectedImpl<U, G, SP>&&> &&
                                !std::is_constructible_v<T, const ExpectedImpl<U, G, SP>&&> &&
                                !std::is_convertible_v<ExpectedImpl<U, G, SP>&, T> &&
                                !std::is_convertible_v<const ExpectedImpl<U, G, SP>&, T> &&
                                !std::is_convertible_v<ExpectedImpl<U, G, SP>&&, T> &&
                                !std::is_convertible_v<const ExpectedImpl<U, G, SP>&&, T>>>
explicit ExpectedImpl(const ExpectedImpl<U, G, SP>& other)
```

**After (using helper concept):**
```cpp
// Helper concept for "not constructible/convertible from Expected"
template <typename T, typename Other>
concept NotConstructibleFromExpected = 
    !std::constructible_from<T, Other&> &&
    !std::constructible_from<T, const Other&> &&
    !std::constructible_from<T, Other&&> &&
    !std::constructible_from<T, const Other&&> &&
    !std::convertible_to<Other&, T> &&
    !std::convertible_to<const Other&, T> &&
    !std::convertible_to<Other&&, T> &&
    !std::convertible_to<const Other&&, T>;

// Then the constructor becomes:
template <typename U, typename G, template <typename, typename> class SP>
    requires std::constructible_from<T, const U&> && 
             std::constructible_from<E, const G&> &&
             NotConstructibleFromExpected<T, ExpectedImpl<U, G, SP>>
explicit ExpectedImpl(const ExpectedImpl<U, G, SP>& other)
```

**Lines saved: 7 per constructor (used twice)**

---

## 4. Full Simplification Inventory

### 4.1 enable_if → requires (21 instances)

| Location | Pattern | Complexity |
|----------|---------|------------|
| Line 316-318 | unexpected constructor | Simple (3 conditions) |
| Line 327 | unexpected in_place ctor | Simple (1 condition) |
| Line 902 | VariantStorage default ctor | Simple (1 condition) |
| Line 1142 | ExpectedImpl default ctor | Simple (1 condition) |
| Line 1158-1160 | Value copy ctor | Medium (3 conditions) |
| Line 1171-1173 | Value move ctor | Medium (3 conditions) |
| Line 1183 | In-place value ctor | Simple (1 condition) |
| Line 1195 | In-place value ctor (init_list) | Simple (1 condition) |
| Line 1208 | In-place error ctor | Simple (1 condition) |
| Line 1220 | In-place error ctor (init_list) | Simple (1 condition) |
| Line 1234 | unexpected copy ctor | Simple (1 condition) |
| Line 1245 | unexpected move ctor | Simple (1 condition) |
| Line 1294-1302 | Converting copy ctor | **Complex (10 conditions)** |
| Line 1322-1330 | Converting move ctor | **Complex (10 conditions)** |
| Line 1440-1441 | Value assignment | Medium (3 conditions) |
| Line 3253-3255 | Void Expected value ctor | Medium (3 conditions) |
| Line 3261 | Void Expected in_place ctor | Simple (1 condition) |

### 4.2 Trait Structs → Concepts (4 traits)

| Trait | Lines | Can Simplify? |
|-------|-------|---------------|
| `is_expected_compatible` | 6 | Yes → concept |
| `is_expected_with_value` | 6 | Yes → concept |
| `is_expected` | 4 + specializations | Yes → concept |
| `is_expected_like` | 4 + specializations | Yes → concept |

---

## 5. Documentation Updates

### 5.1 Remove C++17 Claims

| Line | Current | Updated |
|------|---------|---------|
| 48 | "C++17 compatible, C++20/23 enhanced" | "C++20 minimum, C++23 enhanced" |
| 51 | "C++17: Full functionality" | Remove line |
| 52 | "C++20: + Three-way comparison" | "C++20: Full functionality (base)" |
| 122 | "Feature Test Macros (C++17 Standard Practice)" | "Feature Test Macros" |
| 171 | "Rebind template member (C++17)" | "Rebind template member" |
| 3635 | "C++17 CTAD Deduction Guides" | "CTAD Deduction Guides" |

---

## 6. Implementation Summary

### 6.1 Changes by Category

| Category | Changes | Line Impact |
|----------|---------|-------------|
| enable_if → requires | 21 conversions | -40 to -60 lines |
| SFINAE traits → concepts | 4 conversions | -20 lines |
| Documentation updates | 6 fixes | 0 lines |
| Add helper concepts | 2-3 concepts | +15 lines |
| **Net** | | **-45 to -65 lines** |

### 6.2 New Helper Concepts to Add

```cpp
namespace detail {

/// Type is not constructible/convertible from an Expected type
template <typename T, typename ExpectedType>
concept NotConstructibleFromExpected = 
    !std::constructible_from<T, ExpectedType&> &&
    !std::constructible_from<T, const ExpectedType&> &&
    !std::constructible_from<T, ExpectedType&&> &&
    !std::constructible_from<T, const ExpectedType&&> &&
    !std::convertible_to<ExpectedType&, T> &&
    !std::convertible_to<const ExpectedType&, T> &&
    !std::convertible_to<ExpectedType&&, T> &&
    !std::convertible_to<const ExpectedType&&, T>;

/// Type is not a special tag type
template <typename T, typename Self>
concept NotSpecialType = 
    !std::same_as<std::remove_cvref_t<T>, std::in_place_t> &&
    !std::same_as<std::remove_cvref_t<T>, Self>;

} // namespace detail
```

---

## 7. Side-by-Side Example

### Before (Lines 1158-1165)

```cpp
/**
 * @brief Copy constructs from T.
 * @param v Const reference to T.
 */
template <typename U = T,
          typename = std::enable_if_t<std::is_constructible_v<T, const U&> &&
                                      !std::is_same_v<std::decay_t<U>, std::in_place_t> &&
                                      !std::is_same_v<std::decay_t<U>, ExpectedImpl>>>
constexpr ExpectedImpl(const U& v) noexcept(std::is_nothrow_constructible_v<T, const U&>)
{
    mStorage.store_value(v);
}
```

### After

```cpp
/**
 * @brief Copy constructs from T.
 * @param v Const reference to T.
 */
template <typename U = T>
    requires std::constructible_from<T, const U&> &&
             detail::NotSpecialType<U, ExpectedImpl>
constexpr ExpectedImpl(const U& v) noexcept(std::is_nothrow_constructible_v<T, const U&>)
{
    mStorage.store_value(v);
}
```

**Improvement:**
- Cleaner syntax
- Better error messages
- Reusable constraint via `NotSpecialType`

---

## 8. Benefits

### 8.1 Better Error Messages

**SFINAE failure:**
```
error: no matching function for call to 'ExpectedImpl<int, std::string>::ExpectedImpl(std::in_place_t)'
note: candidate template ignored: requirement 'std::is_constructible_v<int, std::in_place_t>' was not satisfied
```

**Concept failure:**
```
error: no matching function for call to 'ExpectedImpl<int, std::string>::ExpectedImpl(std::in_place_t)'
note: constraints not satisfied
note: because 'detail::NotSpecialType<std::in_place_t, ExpectedImpl<int, std::string>>' evaluated to false
note: because 'std::same_as<std::in_place_t, std::in_place_t>' evaluated to true
```

### 8.2 Reusable Constraints

The `NotConstructibleFromExpected` and `NotSpecialType` concepts can be reused across multiple constructors instead of repeating the same conditions.

### 8.3 Composability

```cpp
// Can combine concepts naturally
template <typename U>
    requires std::constructible_from<T, U> && 
             detail::NotSpecialType<U, ExpectedImpl> &&
             (!Expected<U>)  // Also not another Expected
constexpr ExpectedImpl(U&& v);
```

---

## 9. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Constraint logic error | Low | High | Extensive unit tests exist |
| ABI change | None | N/A | Same signatures, different SFINAE |
| Missing overload | Low | Medium | Tests catch this immediately |

---

## 10. Summary

### What Changes

| Item | Before | After |
|------|--------|-------|
| `enable_if` patterns | 21 | 0 |
| SFINAE trait structs | 4 | 0 (concepts instead) |
| Complex 10-line constraints | 2 | 2 helper concepts |
| Documentation | Claims C++17 | Claims C++20 |

### What Stays the Same

- All 22 feature test macros (legitimate)
- TRY helper macros (require `__LINE__`)
- C++23 `std::expected` integration guards
- Exception detection logic
- Public API signatures

### Estimated Impact

- **Lines removed:** 45-65
- **Readability:** Significantly improved
- **Error messages:** Much better
- **Effort:** 2-3 hours
- **Risk:** Low

---

*End of Document*
