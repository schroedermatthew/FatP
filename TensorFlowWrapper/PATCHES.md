# TensorFlow C++20 Wrapper - Patch v4.1

## Summary

This patch addresses all remaining issues identified in the ChatGPT review comparison, plus additional issues found during code review.

---

## Fixes Applied

### P0-C: View Lifetime Safety (CRITICAL)

**Problem:** Views returned by `read<T>()` and `write<T>()` could outlive the `Tensor` object, causing use-after-free:

```cpp
auto v = [] {
    Tensor<policy::SharedMutex> t = Tensor<...>::FromVector(...);
    return t.read<float>();  // Returns view, t destroyed after return
}();
// v now holds span pointing to freed memory AND lock on freed mutex → UB
```

**Solution:** Introduced `TensorState<Policy>` and `TensorView<T, Policy, Guard>`:

- `Tensor` now stores a `shared_ptr<TensorState<Policy>>` instead of raw members
- `TensorView` (returned by `read()`/`write()`) captures this `shared_ptr`
- The tensor data and mutex remain alive as long as ANY view exists

```cpp
// Now SAFE - view keeps tensor alive
auto v = [] {
    Tensor<policy::SharedMutex> t = Tensor<...>::FromVector(...);
    return t.read<float>();
}();
// v.state_ holds shared_ptr → tensor data still valid
```

**Files changed:** `tensor.hpp`

---

### P0-D: TF_NewTensor Failure Memory Leak

**Problem:** If `TF_NewTensor` failed, the allocated data buffer was leaked:

```cpp
tensor_ = TF_NewTensor(..., data, ...);
if (!tensor_) {
    throw std::runtime_error("...");  // BUG: data leaked!
}
```

**Solution:** Added `DataGuard` RAII helper in `create_tensor()` that ensures data is deallocated on any exception path - including exceptions thrown BEFORE `TF_NewTensor` is called (e.g., during vector construction).

**Files changed:** `tensor.hpp` (centralized in `create_tensor()`)

---

### P1: Dangerous Default Deallocator

**Problem:** The raw constructor defaulted deallocator to `nullptr` → `free()`:

```cpp
Tensor(dtype, dims, void* data, size_t bytes,
       void (*deallocator)(...) = nullptr,  // Dangerous!
       ...)
```

Users could accidentally pass `new[]`-allocated or stack memory and get silent UB when `free()` was called.

**Solution:** Replaced with explicit factory methods:

| Method | Use Case |
|--------|----------|
| `Tensor::FromVector(dims, vec)` | Copy data (safest) |
| `Tensor::Allocate<T>(dims)` | Uninitialized malloc'd memory |
| `Tensor::Zeros<T>(dims)` | Zero-initialized calloc'd memory |
| `Tensor::AdoptMalloc<T>(dims, data, bytes)` | Adopt malloc'd buffer |
| `Tensor::Adopt(dtype, dims, data, bytes, deallocator)` | Adopt with REQUIRED deallocator |

The dangerous raw constructor has been removed from the public API.

**Files changed:** `tensor.hpp`

---

### P1: Re-entrancy Deadlock Documentation

**Problem:** With `policy::Mutex`, holding a view while calling `Session::Run()` with the same tensor deadlocks:

```cpp
Tensor<policy::Mutex> input = ...;
auto view = input.read<float>();   // Acquires EXCLUSIVE lock
session.Run({Feed{"x", input}}, ...);  // → DEADLOCK if Run tried to lock
```

**Solution:** Added comprehensive documentation in `session.hpp` with:
- Clear explanation of the hazard
- ASCII art warning box
- Four safe patterns with code examples
- Guidance on which policy to use

**Files changed:** `session.hpp`

---

## Additional Fixes (Found During Code Review)

### Missing `<iterator>` Include

**Problem:** `TensorView` uses `std::reverse_iterator` but `<iterator>` was not included.

**Solution:** Added `#include <iterator>` to tensor.hpp.

---

### Incorrect `noexcept` on Default Constructor

**Problem:** `Tensor() noexcept` was marked noexcept, but `std::make_shared` can throw `std::bad_alloc`.

**Solution:** Removed `noexcept` specifier from default constructor.

---

### Null Data Validation

**Problem:** `AdoptMalloc` and `Adopt` didn't validate that `data` is non-null when `byte_len > 0`.

**Solution:** Added validation that throws `std::invalid_argument` if `data` is null with non-zero `byte_len`.

---

### Integer Overflow in Size Calculations

**Problem:** Dimension multiplication could overflow silently for very large tensors:

```cpp
std::size_t expected = 1;
for (auto d : dims) {
    expected *= static_cast<std::size_t>(d);  // No overflow check!
}
```

**Solution:** Added `detail::checked_mul()` and `detail::checked_product()` helpers that throw `std::overflow_error` on overflow. C++20 doesn't have `std::checked_*` (that's C++26), so we implement manually:

```cpp
if (b > std::numeric_limits<std::size_t>::max() / a) {
    throw std::overflow_error(...);
}
```

All factory methods (`FromVector`, `Allocate`, `Zeros`) now use these checked functions.

---

### FromScalar Creates Wrong Shape

**Problem:** `FromScalar` created a tensor with shape `[1]` (rank 1, 1 element), but TensorFlow scalars should have shape `[]` (rank 0).

```cpp
// Before: Creates shape [1] - NOT a true scalar
static constexpr std::int64_t dims_arr[1] = {1};
return FromVector<T>(std::span<const std::int64_t>(dims_arr, 1), ...);
```

**Solution:** `FromScalar` now creates a true scalar with empty shape:

```cpp
// After: Creates shape [] - true TensorFlow scalar
return create_tensor(tf_dtype_v<T>, std::span<const std::int64_t>{}, ...);
```

---

### Moved-from Tensor Crashes

**Problem:** After `Tensor t2 = std::move(t1)`, calling any method on `t1` would crash because `state_` was null (default `shared_ptr` move behavior).

**Solution:** Tensor move operations now leave the source in a valid empty state:

```cpp
Tensor(Tensor&& other)
    : state_(std::move(other.state_))
{
    // Leave other in valid empty state (like std::vector)
    other.state_ = std::make_shared<TensorState<Policy>>();
}
```

Note: Move is no longer `noexcept` because `make_shared` can throw.

---

### Moved-from Session Crashes on Run()

**Problem:** After moving a Session, calling `Run()` would pass nullptr to TensorFlow.

**Solution:** Added null check at start of `Run()`:

```cpp
if (!session_) {
    throw std::runtime_error("Session::Run(): session is null (moved-from?)");
}
```

---

## Files Modified

| File | Changes |
|------|---------|
| `tensor.hpp` | TensorState, TensorView, DataGuard RAII, leak fixes, explicit APIs, iterator include, noexcept fix, null validation, **overflow checking**, **FromScalar true scalar**, **safe moved-from state** |
| `session.hpp` | Added re-entrancy documentation, extracted Cleanup() helper, **null check in Run()** |
| `guarded_span.hpp` | Updated header comment noting TensorView preference |

---

## API Changes

### New Types

```cpp
// Internal shared state (not directly used by clients)
template<class Policy>
struct TensorState;

// View type returned by read()/write() - now lifetime-safe
template<TensorScalar T, class Policy, class Guard>
class TensorView;
```

### Removed

```cpp
// REMOVED: Dangerous constructor with default deallocator
Tensor(TF_DataType dtype, std::span<const std::int64_t> dims,
       void* data, std::size_t byte_len,
       void (*deallocator)(...) = nullptr,  // No longer public
       void* deallocator_arg = nullptr);
```

### Added

```cpp
// Adopt malloc'd memory explicitly
template<TensorScalar T>
static Tensor AdoptMalloc(std::span<const std::int64_t> dims,
                          void* data, std::size_t byte_len);

// Adopt with required deallocator
static Tensor Adopt(TF_DataType dtype,
                    std::span<const std::int64_t> dims,
                    void* data, std::size_t byte_len,
                    Deallocator deallocator,  // REQUIRED
                    void* deallocator_arg = nullptr);
```

---

## Migration Guide

### Before (v3)

```cpp
// Potentially dangerous - what if data was new[]'d?
void* data = std::malloc(bytes);
Tensor<> t(TF_FLOAT, dims, data, bytes);  // Implicit free()
```

### After (v4)

```cpp
// Option 1: Let Tensor manage allocation (safest)
auto t = Tensor<>::FromVector<float>(dims, my_vector);

// Option 2: Explicit malloc adoption
void* data = std::malloc(bytes);
auto t = Tensor<>::AdoptMalloc<float>(dims, data, bytes);

// Option 3: Custom deallocator (required!)
void* data = my_allocator.alloc(bytes);
auto t = Tensor<>::Adopt(TF_FLOAT, dims, data, bytes,
    [](void* d, size_t, void* ctx) {
        static_cast<MyAllocator*>(ctx)->free(d);
    },
    &my_allocator);
```

---

## Thread Safety Summary

| Pattern | Safe? | Notes |
|---------|-------|-------|
| `read()` then `Run()` (same tensor, Mutex) | ❌ | Deadlock |
| `read()` then `Run()` (same tensor, SharedMutex) | ✓ | OK - shared lock |
| `read()` then `Run()` (different tensors) | ✓ | No conflict |
| Return view from function | ✓ | View keeps tensor alive (v4 fix) |
| `with_read()` callback pattern | ✓ | Always safe |
| Multiple threads reading (SharedMutex) | ✓ | Concurrent reads OK |
| Multiple threads writing | ❌ | Always serialize writes |

---

## Testing Recommendations

1. **Compile instantiation test**: Verify all templates compile with NoLock, Mutex, SharedMutex
2. **View lifetime test**: Return a view from a lambda, verify data is still valid
3. **Leak test**: Cause TF_NewTensor to fail (e.g., invalid dims), verify no leak with valgrind
4. **Deadlock test**: Verify documented patterns work/fail as expected

---

## Credits

- **ChatGPT**: Identified P0-C (view lifetime), P0-D (leak), P1 (default deallocator), P1 (re-entrancy)
- **Claude**: Original merged implementation, P0-C shared state design, documentation
