# C++17 Documentation Drift & Fallback Code Cleanup Plan

**Document Version:** 1.0  
**Date:** 2026-02-03  
**Status:** Proposed  
**Reference:** Fat-P Guidelines §1.1.1 (C++20 Minimum Standard)

---

## 1. Executive Summary

A comprehensive audit of Fat-P headers identified **75 instances** of "C++17" mentions across **43 files**. This document categorizes these instances and provides an implementation plan to align documentation and code with the C++20 minimum standard policy established in §1.1.1.

### Key Metrics

| Category | Count | Action |
|----------|-------|--------|
| Documentation drift ("Requires: C++17") | 31 files | Update to "Requires: C++20" |
| Active fallback code | 4 files | Remove C++17 fallbacks |
| Historical/explanatory comments | 7 instances | Review case-by-case |
| Already fixed this session | 4 files | No action needed |

### Files Already Fixed (Excluded from Plan)

- ConstexprUtilities.h — Removed `FATP_CONSTEVAL` macro
- SmallVector.h — Removed `[[no_unique_address]]` and `<memory_resource>` guards
- MemoryMappedFile.h — Removed custom `span` implementation
- JsonLite.h — Replaced custom `SourceLocation` with `std::source_location`

---

## 2. Category 1: Documentation Drift

### 2.1 Problem Statement

31 header files contain `Requires: C++17` in their Doxygen documentation, contradicting the C++20 minimum policy in §1.1.1.

### 2.2 Affected Files

| # | File | Line | Current Text |
|---|------|------|--------------|
| 1 | CacheUtilities.h | L69 | `* Requires: C++17` |
| 2 | DebugOnly.h | L60 | `* Requires: C++17` |
| 3 | DiagnosticLogger_Core.h | L64 | `* Requires: C++17` |
| 4 | enforce_enforcers.h | L41 | `* Requires: C++17` |
| 5 | EnforcedInit.h | L72 | `* Requires: C++17` |
| 6 | Expected.h | L71 | `* Requires: C++17 (C++20 recommended)` |
| 7 | FatPBenchmarkRunner.h | L71 | `* Requires: C++17` |
| 8 | FloatingPointComparison.h | L72 | `* Requires: C++17` |
| 9 | HpcVector.h | L95 | `* Requires: C++17` |
| 10 | IdGenerator.h | L54 | `* Requires: C++17` |
| 11 | LockFreeRingBuffer.h | L78 | `* Requires: C++17` |
| 12 | NumaAlignedAllocator.h | L60 | `* Requires: C++17` |
| 13 | NumaAllocator.h | L62 | `* Requires: C++17` |
| 14 | PipeOperator.h | L57 | `* Requires: C++17` |
| 15 | RateLimiter.h | L70 | `* Requires: C++17` |
| 16 | Reflection.h | L69 | `* Requires: C++17` |
| 17 | ScopeGuard.h | L60 | `* Requires: C++17` |
| 18 | ScopeGuardExpected.h | L61 | `* Requires: C++17` |
| 19 | ScopeGuardPolicies.h | L61 | `* Requires: C++17` |
| 20 | Signal.h | L76 | `* Requires: C++17` |
| 21 | SlidingFileWindow.h | L59 | `* Requires: C++17` |
| 22 | StableHashMap.h | L94 | `* Requires: C++17` |
| 23 | Stacktrace.h | L73 | `* Requires: C++17` |
| 24 | StringPool.h | L67 | `* Requires: C++17` |
| 25 | Tensor.h | L83 | `* Requires: C++17` |
| 26 | TensorMath.h | L56 | `* Requires: C++17` |
| 28 | TensorSerializer.h | L63 | `* Requires: C++17` |
| 29 | TensorStorage.h | L67 | `* Requires: C++17` |
| 30 | ValueGuard.h | L58 | `* Requires: C++17` |
| 31 | ViewLifetimeTracking.h | L63 | `* Requires: C++17` |

### 2.3 Resolution

**Change:** `Requires: C++17` → `Requires: C++20`

**Implementation:** Batch sed replacement (see Section 5.1)

---

## 3. Category 2: Active Fallback Code

### 3.1 Problem Statement

4 header files contain actual C++17 compatibility code that should be removed per §1.1.4 ("No C++17 fallback code paths").

### 3.2 BinaryLite.h

**Location:** Line 69

**Current Code:**
```cpp
// C++20 has std::endian; for C++17 we use compiler intrinsics
#if defined(__cpp_lib_endian) && __cpp_lib_endian >= 201907L
#include <bit>
using std::endian;
#else
enum class endian
{
#if defined(_MSC_VER) && !defined(__clang__)
    little = 0,
    big    = 1,
    native = little
#else
    little = __ORDER_LITTLE_ENDIAN__,
    big    = __ORDER_BIG_ENDIAN__,
    native = __BYTE_ORDER__
#endif
};
#endif
```

**Target Code:**
```cpp
#include <bit>
// std::endian is guaranteed in C++20
```

**Rationale:** `std::endian` is part of C++20 `<bit>` header. No fallback needed.

---

### 3.3 CacheUtilities.h

**Location:** Line 574

**Current Code:**
```cpp
/**
 * Uses template specialization for C++17 compatibility (avoids [[no_unique_address]]
 */
template <typename T, typename Tag, bool = std::is_empty_v<Tag>>
struct CompressedPair;

template <typename T, typename Tag>
struct CompressedPair<T, Tag, true> : private Tag  // EBO inheritance trick
{
    T value;
    // ...
};

template <typename T, typename Tag>
struct CompressedPair<T, Tag, false>
{
    T value;
    Tag tag;
    // ...
};
```

**Target Code:**
```cpp
/**
 * Compressed pair using [[no_unique_address]] for empty tag optimization
 */
template <typename T, typename Tag>
struct CompressedPair
{
    T value;
    [[no_unique_address]] Tag tag;
    // ...
};
```

**Rationale:** `[[no_unique_address]]` is guaranteed in C++20. The EBO inheritance workaround is unnecessary complexity.

---

### 3.4 AtomicSharedPtr.h

**Location:** Line 173

**Current Code:**
```cpp
// Wait/Notify (C++20 only - not available on C++17)
#if FATP_HAS_ATOMIC_WAIT
    void wait(std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        mAtomic.wait(mAtomic.load(order), order);
    }
    
    void notify_one() noexcept { mAtomic.notify_one(); }
    void notify_all() noexcept { mAtomic.notify_all(); }
#endif
```

**Target Code:**
```cpp
void wait(std::memory_order order = std::memory_order_seq_cst) const noexcept
{
    mAtomic.wait(mAtomic.load(order), order);
}

void notify_one() noexcept { mAtomic.notify_one(); }
void notify_all() noexcept { mAtomic.notify_all(); }
```

**Rationale:** `std::atomic::wait/notify_one/notify_all` are guaranteed in C++20. Remove the `#if` guard.

---

### 3.5 Reflection.h

**Location:** Line 34

**Current Code:**
```cpp
/**
 * @version 3.0.1 - Unified REFLECT_REGISTER for both C++17 and C++20 (MSVC-compatible)
 */

// Later in file:
#if __cpp_nontype_template_args >= 201911L
    // C++20 path with string literal NTTP
#else
    // C++17 path with __COUNTER__ workaround
#endif
```

**Target Code:**
```cpp
/**
 * @version 4.0.0 - C++20 only, uses string literal NTTP
 */

// Use C++20 path only - string literal non-type template parameters
```

**Rationale:** C++20 guarantees string literal NTTPs (`__cpp_nontype_template_args >= 201911L`). The `__COUNTER__` workaround for C++17 should be removed.

**Note:** This file requires careful review as the reflection macros are complex.

---

## 4. Category 3: Historical/Explanatory Comments

### 4.1 Comments to Update

| File | Line | Current | Target |
|------|------|---------|--------|
| ContractException.h | L79 | `C++17 minimum required for "recoverable"` | `C++20 minimum required` |
| CppFeatureDetection.h | L444 | `Required feature-test macros are C++17+` | `Required feature-test macros are C++20+` |
| FatPConfig.h | L134 | `// C++17 / C++20: feature gates` | `// C++20 / C++23: feature gates` |

### 4.2 Comments to Keep (Factual/Historical)

| File | Line | Text | Reason to Keep |
|------|------|------|----------------|
| AllocationStrategies.h | L74 | `C++17 aligned new/delete` | Factual - C++17 introduced this feature |
| AllocationStrategies.h | L159 | `C++17 new handles over-aligned structs` | Factual - explains language behavior |
| CircularBuffer.h | L52 | `C++17 provides std::hardware_destructive_interference_size but:` | Explains why feature not used |
| CppFeatureDetection.h | L52 | `C++17: 201703L` | Documents standard version macro values |

### 4.3 Comments to Review

| File | Line | Text | Review Reason |
|------|------|------|---------------|
| CheckedArithmeticInt.h | L93 | `C++17 constexpr functions may not contain non-literal locals` | May be obsolete with C++20 relaxed constexpr |

---

## 5. Implementation Plan

### 5.1 Phase 1: Documentation Updates

**Scope:** 31 files  
**Effort:** 15-20 minutes  
**Risk:** Very Low

**Automated Script:**
```bash
#!/bin/bash
# update_cpp_requirements.sh
# Run from repository root

HEADER_DIR="include/fat_p"

FILES=(
    "CacheUtilities.h"
    "DebugOnly.h"
    "DiagnosticLogger_Core.h"
    "enforce_enforcers.h"
    "EnforcedInit.h"
    "Expected.h"
    "FatPBenchmarkRunner.h"
    "FloatingPointComparison.h"
    "HpcVector.h"
    "IdGenerator.h"
    "LockFreeRingBuffer.h"
    "NumaAlignedAllocator.h"
    "NumaAllocator.h"
    "PipeOperator.h"
    "RateLimiter.h"
    "Reflection.h"
    "ScopeGuard.h"
    "ScopeGuardExpected.h"
    "ScopeGuardPolicies.h"
    "Signal.h"
    "SlidingFileWindow.h"
    "StableHashMap.h"
    "Stacktrace.h"
    "StringPool.h"
    "Tensor.h"
    "TensorMath.h"
    "TensorSerializer.h"
    "TensorStorage.h"
    "ValueGuard.h"
    "ViewLifetimeTracking.h"
)

for file in "${FILES[@]}"; do
    filepath="${HEADER_DIR}/${file}"
    if [[ -f "$filepath" ]]; then
        sed -i 's/Requires: C++17/Requires: C++20/g' "$filepath"
        echo "Updated: $file"
    else
        echo "WARNING: File not found: $filepath"
    fi
done

echo "Phase 1 complete: ${#FILES[@]} files processed"
```

**Verification:**
```bash
grep -rn "Requires: C++17" include/fat_p/
# Should return 0 results after execution
```

---

### 5.2 Phase 2: Fallback Code Removal

**Scope:** 4 files  
**Effort:** 2-3 hours  
**Risk:** Medium (requires testing)

#### 5.2.1 BinaryLite.h

| Step | Action |
|------|--------|
| 1 | Locate endian detection block (~L69) |
| 2 | Remove `#if defined(__cpp_lib_endian)` guard |
| 3 | Remove custom `enum class endian` definition |
| 4 | Add `#include <bit>` if not present |
| 5 | Verify all uses of `endian::native`, `endian::little`, `endian::big` |

#### 5.2.2 CacheUtilities.h

| Step | Action |
|------|--------|
| 1 | Locate `CompressedPair` template (~L574) |
| 2 | Remove template specializations for EBO |
| 3 | Replace with single template using `[[no_unique_address]]` |
| 4 | Update documentation comment |
| 5 | Test with empty and non-empty tag types |

#### 5.2.3 AtomicSharedPtr.h

| Step | Action |
|------|--------|
| 1 | Locate wait/notify section (~L173) |
| 2 | Remove `#if FATP_HAS_ATOMIC_WAIT` guard |
| 3 | Remove corresponding `#endif` |
| 4 | Ensure `wait()`, `notify_one()`, `notify_all()` are unconditional |

#### 5.2.4 Reflection.h

| Step | Action |
|------|--------|
| 1 | Audit all `#if __cpp_nontype_template_args` blocks |
| 2 | Keep only C++20 path (NTTP with string literals) |
| 3 | Remove `__COUNTER__` workarounds |
| 4 | Update `@version` comment |
| 5 | Comprehensive test of all `REFLECT_*` macros |

---

### 5.3 Phase 3: Comment Cleanup

**Scope:** 3-4 files  
**Effort:** 30 minutes  
**Risk:** Very Low

| File | Line | Change |
|------|------|--------|
| ContractException.h | L79 | `C++17 minimum` → `C++20 minimum` |
| CppFeatureDetection.h | L444 | `C++17+` → `C++20+` |
| FatPConfig.h | L134 | `C++17 / C++20` → `C++20 / C++23` |

---

### 5.4 Phase 4: Verification

**Scope:** All modified files  
**Effort:** 1 hour  
**Risk:** Low

#### 5.4.1 Grep Verification

```bash
# Find remaining C++17 mentions (should only be historical/factual)
grep -rn "C++17" include/fat_p/ | grep -v \
    -e "C++17 introduced" \
    -e "C++17 added" \
    -e "C++17 provides" \
    -e "since C++17" \
    -e "201703L"

# Expected: 0 results for "Requires: C++17" or compatibility claims
```

#### 5.4.2 Compilation Test

```bash
# Compile with strict C++20, no fallbacks should trigger
g++ -std=c++20 -Wall -Wextra -pedantic \
    -DNDEBUG \
    -I include \
    tests/test_all.cpp -o /dev/null
```

#### 5.4.3 Static Analysis

- Run `fatp_meta_inventory.py` to verify FATP_META consistency
- Check that no new schema failures introduced

---

## 6. Risk Assessment

### 6.1 Phase 1 Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Typo in sed command | Low | Low | Review diff before commit |
| Missed files | Low | Low | Grep verification catches this |

### 6.2 Phase 2 Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| BinaryLite endian logic error | Low | Medium | Unit tests for byte swapping |
| CompressedPair size regression | Medium | Low | `static_assert(sizeof(...))` tests |
| AtomicSharedPtr wait semantics | Low | Medium | Concurrency tests |
| Reflection macro breakage | Medium | High | Comprehensive reflection tests |

### 6.3 Rollback Plan

All changes are to header files under version control. Rollback procedure:
```bash
git checkout HEAD~1 -- include/fat_p/
```

---

## 7. Timeline

| Phase | Dependencies | Duration | Owner |
|-------|--------------|----------|-------|
| Phase 1: Doc updates | None | 20 min | — |
| Phase 2: Code removal | Phase 1 | 2-3 hrs | — |
| Phase 3: Comments | Phase 1 | 30 min | — |
| Phase 4: Verification | Phase 2, 3 | 1 hr | — |
| **Total** | | **4-5 hrs** | |

---

## 8. Appendix: Complete C++17 Mention Inventory

### A.1 Files with "Requires: C++17" (31 files)

```
CacheUtilities.h:69
DebugOnly.h:60
DiagnosticLogger_Core.h:64
enforce_enforcers.h:41
EnforcedInit.h:72
Expected.h:71
FatPBenchmarkRunner.h:71
FloatingPointComparison.h:72
HpcVector.h:95
IdGenerator.h:54
LockFreeRingBuffer.h:78
NumaAlignedAllocator.h:60
NumaAllocator.h:62
PipeOperator.h:57
RateLimiter.h:70
Reflection.h:69
ScopeGuard.h:60
ScopeGuardExpected.h:61
ScopeGuardPolicies.h:61
Signal.h:76
SlidingFileWindow.h:59
StableHashMap.h:94
Stacktrace.h:73
StringPool.h:67
Tensor.h:83
TensorMath.h:56
TensorSerializer.h:63
TensorStorage.h:67
ValueGuard.h:58
ViewLifetimeTracking.h:63
```

### A.2 Files with Active C++17 Fallback Code (4 files)

```
BinaryLite.h:69         - std::endian fallback
CacheUtilities.h:574    - [[no_unique_address]] workaround
AtomicSharedPtr.h:173   - atomic wait/notify guard
Reflection.h:34         - NTTP string literal workaround
```

### A.3 Files with Historical/Explanatory Comments (Keep)

```
AllocationStrategies.h:74   - "C++17 aligned new/delete"
AllocationStrategies.h:159  - "C++17 new handles over-aligned"
CircularBuffer.h:52         - "C++17 provides std::hardware_destructive_interference_size"
CppFeatureDetection.h:52    - "C++17: 201703L"
```

### A.4 Files Already Fixed This Session (Excluded)

```
ConstexprUtilities.h  - Removed FATP_CONSTEVAL macro
SmallVector.h         - Removed [[no_unique_address]] guard, <memory_resource> guard
MemoryMappedFile.h    - Removed custom span implementation
JsonLite.h            - Replaced SourceLocation with std::source_location
```

---

*End of Document*
