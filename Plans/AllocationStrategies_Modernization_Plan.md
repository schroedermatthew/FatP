# AllocationStrategies.h Modernization Plan

**Document Version:** 1.0  
**Date:** 2026-02-03  
**Status:** Proposed  
**File:** `include/fat_p/AllocationStrategies.h`  
**Lines:** 549  
**Current Macros:** 0

---

## 1. Executive Summary

AllocationStrategies.h contains **unnecessary complexity** in `NewDeleteAllocator`. The `if constexpr (kIsOveraligned)` branching manually reimplements what C++17 new-expressions already do automatically. Since Fat-P requires C++20, this code can be **dramatically simplified**.

### Key Finding

```cpp
// CURRENT: 20 lines of unnecessary branching
if constexpr (kIsOveraligned)
{
    void* mem = ::operator new(sizeof(T), std::align_val_t{alignof(T)});
    return new (mem) T(std::forward<Args>(args)...);
}
else
{
    return new T(std::forward<Args>(args)...);
}

// SIMPLIFIED: 1 line - C++17 new handles this automatically
return new T(std::forward<Args>(args)...);
```

### Impact

| Metric | Before | After |
|--------|--------|-------|
| Lines in NewDeleteAllocator | 60 | ~35 |
| `if constexpr` branches | 2 | 0 |
| Dead code (`kIsOveraligned`) | 1 constant | 0 |
| Cognitive complexity | Medium | Low |

---

## 2. Technical Background

### C++17 Aligned Allocation (P0035R4)

Since C++17, new-expressions **automatically** handle over-aligned types:

> When allocating an object whose alignment exceeds `__STDCPP_DEFAULT_NEW_ALIGNMENT__`, the implementation calls `::operator new(size_t, std::align_val_t)` instead of `::operator new(size_t)`.

This means:
```cpp
struct alignas(64) CacheAligned { int data; };

// C++17+: Compiler automatically calls aligned new
auto* p = new CacheAligned();  // Uses ::operator new(size, align_val_t{64})
delete p;                       // Uses ::operator delete(ptr, align_val_t{64})
```

The manual `if constexpr` check in AllocationStrategies.h **duplicates** this automatic behavior.

### Why the Original Code Exists

The pattern was likely written for C++14 compatibility, where:
- `std::align_val_t` didn't exist
- Over-aligned `new` wasn't automatic
- Manual alignment handling was required

With C++20 minimum, this complexity is pure dead weight.

---

## 3. Current Code Analysis

### 3.1 NewDeleteAllocator::allocate() (Lines 112-126)

**Current:**
```cpp
static constexpr bool kIsOveraligned = alignof(T) > alignof(std::max_align_t);

template <typename... Args>
T* allocate(Args&&... args)
{
    if constexpr (kIsOveraligned)
    {
        // Over-aligned: use aligned allocation + placement new
        void* mem = ::operator new(sizeof(T), std::align_val_t{alignof(T)});
        return new (mem) T(std::forward<Args>(args)...);
    }
    else
    {
        // Normal alignment: standard new handles it
        return new T(std::forward<Args>(args)...);
    }
}
```

**Problem:** Both branches produce identical machine code on C++17+. The compiler already does this selection.

**Simplified:**
```cpp
template <typename... Args>
T* allocate(Args&&... args)
{
    return new T(std::forward<Args>(args)...);
}
```

### 3.2 NewDeleteAllocator::deallocate() (Lines 136-148)

**Current:**
```cpp
void deallocate(T* ptr)
{
    if constexpr (kIsOveraligned)
    {
        // Over-aligned: explicit destructor + aligned delete
        ptr->~T();
        ::operator delete(ptr, std::align_val_t{alignof(T)});
    }
    else
    {
        delete ptr;
    }
}
```

**Problem:** Same issue. `delete ptr` automatically calls the aligned deallocation function for over-aligned types.

**Simplified:**
```cpp
void deallocate(T* ptr)
{
    delete ptr;
}
```

### 3.3 Dead Code

With the simplification, this becomes dead:
```cpp
static constexpr bool kIsOveraligned = alignof(T) > alignof(std::max_align_t);
```

Remove it.

---

## 4. Documentation Updates

### 4.1 Class Documentation (Lines 66-88)

**Current (Lines 73-74):**
```cpp
 * Supports over-aligned types (alignof(T) > alignof(std::max_align_t)) via
 * C++17 aligned new/delete. Examples: SIMD types, cache-line aligned structs.
```

**Updated:**
```cpp
 * Supports over-aligned types (alignof(T) > alignof(std::max_align_t))
 * automatically via C++17+ aligned new/delete.
 * Examples: SIMD types, cache-line aligned structs.
```

### 4.2 BlockAllocator Documentation (Lines 158-159)

**Current:**
```cpp
 * Supports over-aligned types via alignas propagation - the Block struct
 * inherits T's alignment, and C++17 new handles over-aligned structs.
```

**Updated:**
```cpp
 * Supports over-aligned types via alignas propagation - the Block struct
 * inherits T's alignment, and new-expressions handle alignment automatically.
```

---

## 5. Implementation

### 5.1 Simplified NewDeleteAllocator

```cpp
/**
 * @brief Standard new/delete allocator with per-object heap allocation.
 *
 * Simple wrapper around operator new/delete. Each object is individually
 * allocated from the heap. This often provides better cache locality for
 * lookups due to malloc's allocation patterns.
 *
 * Supports over-aligned types (alignof(T) > alignof(std::max_align_t))
 * automatically via C++17+ aligned new/delete.
 * Examples: SIMD types, cache-line aligned structs.
 *
 * @tparam T Element type to allocate.
 *
 * @note Complexity: allocate() O(1) amortized, deallocate() O(1) amortized.
 * @note Memory overhead: malloc metadata per object (~16-32 bytes typical).
 * @note Thread-safety: NOT thread-safe. Caller must synchronize.
 */
template <typename T>
class NewDeleteAllocator
{
public:
    NewDeleteAllocator() = default;
    NewDeleteAllocator(const NewDeleteAllocator&) = default;
    NewDeleteAllocator& operator=(const NewDeleteAllocator&) = default;
    NewDeleteAllocator(NewDeleteAllocator&&) noexcept = default;
    NewDeleteAllocator& operator=(NewDeleteAllocator&&) noexcept = default;
    ~NewDeleteAllocator() = default;

    /**
     * @brief Allocates and constructs an object.
     *
     * @tparam Args Constructor argument types.
     * @param args Arguments forwarded to T's constructor.
     * @return Pointer to newly constructed object.
     *
     * @throws std::bad_alloc if allocation fails.
     * @throws Any exception thrown by T's constructor.
     */
    template <typename... Args>
    T* allocate(Args&&... args)
    {
        return new T(std::forward<Args>(args)...);
    }

    /**
     * @brief Destroys and deallocates an object.
     *
     * @param ptr Pointer to object previously returned by allocate().
     *
     * @pre ptr was returned by allocate() on this allocator.
     * @pre ptr has not already been deallocated.
     */
    void deallocate(T* ptr)
    {
        delete ptr;
    }
};
```

### 5.2 Lines Removed

| Lines | Content | Reason |
|-------|---------|--------|
| 92 | `static constexpr bool kIsOveraligned = ...` | Dead code |
| 115-120 | `if constexpr (kIsOveraligned) { ... }` | Duplicates compiler behavior |
| 121-125 | `else { ... }` | Now the only path |
| 138-143 | `if constexpr (kIsOveraligned) { ... }` | Duplicates compiler behavior |
| 144-147 | `else { ... }` | Now the only path |

**Total lines removed:** ~25

---

## 6. Verification

### 6.1 Compile Test

```cpp
// Test that over-aligned types still work
struct alignas(64) CacheAligned { int x; };
struct alignas(256) SuperAligned { double data[4]; };

fat_p::NewDeleteAllocator<CacheAligned> alloc1;
fat_p::NewDeleteAllocator<SuperAligned> alloc2;

auto* p1 = alloc1.allocate(42);
auto* p2 = alloc2.allocate();

assert(reinterpret_cast<uintptr_t>(p1) % 64 == 0);
assert(reinterpret_cast<uintptr_t>(p2) % 256 == 0);

alloc1.deallocate(p1);
alloc2.deallocate(p2);
```

### 6.2 AddressSanitizer

Run existing tests with ASan to verify no memory issues:
```bash
g++ -std=c++20 -fsanitize=address -g \
    tests/test_AllocationStrategies.cpp -o test_alloc
./test_alloc
```

---

## 7. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Alignment broken | Very Low | High | C++17 guarantees this works |
| Memory leak | Very Low | High | ASan verification |
| ABI change | None | N/A | Internal implementation only |

**Why risk is very low:**

The C++ standard (since C++17) **guarantees** that:
1. `new T(...)` calls aligned allocation for over-aligned T
2. `delete ptr` calls aligned deallocation for over-aligned T

We're removing code that duplicates standard behavior.

---

## 8. Summary

### What Changes

| Item | Before | After |
|------|--------|-------|
| `kIsOveraligned` constant | Present | Removed |
| `allocate()` | 12 lines with `if constexpr` | 1 line |
| `deallocate()` | 11 lines with `if constexpr` | 1 line |
| NewDeleteAllocator total | ~60 lines | ~35 lines |
| Comments referencing "C++17" | 2 | 0 (updated wording) |

### What Does NOT Change

- BlockAllocator implementation (already optimal)
- PoolAllocator implementation (already optimal)
- Public API (allocate/deallocate signatures unchanged)
- Behavior (identical runtime behavior)

### Conclusion

This is a **pure simplification** — removing ~25 lines of unnecessary `if constexpr` branching that duplicates what C++17+ compilers already do automatically. The change has **zero risk** because we're relying on standard-mandated behavior.

**Effort:** 15 minutes  
**Risk:** Very Low  
**Benefit:** Cleaner, more maintainable code

---

*End of Document*
