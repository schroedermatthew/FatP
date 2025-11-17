# FatPTypeTraits User Manual

## Table of Contents

1. [Introduction](#introduction)
2. [What Are Library-Specific Type Traits?](#what-are-library-specific-type-traits)
3. [Architecture](#architecture)
4. [Container Type Detection](#container-type-detection)
5. [Tensor and Mathematical Types](#tensor-and-mathematical-types)
6. [Concurrent Types](#concurrent-types)
7. [Memory Management Types](#memory-management-types)
8. [Utility Types](#utility-types)
9. [Composite Library Traits](#composite-library-traits)
10. [Duck-Typed Detection](#duck-typed-detection)
11. [Policy Detection](#policy-detection)
12. [Diagnostic Utilities](#diagnostic-utilities)
13. [Design-by-Contract Helpers](#design-by-contract-helpers)
14. [C++20 Concepts](#cpp20-concepts)
15. [Integration with TypeTraits](#integration-with-typetraits)
16. [Usage Examples](#usage-examples)
17. [Best Practices](#best-practices)
18. [Performance Considerations](#performance-considerations)

---

## Introduction

The `FatPTypeTraits.h` header provides comprehensive type traits specifically for detecting and working with fat_penelope library components. These traits enable compile-time detection of library types, allowing generic code to optimize for specific container implementations.

### Key Features

- **Forward Declarations Only**: No circular dependencies
- **Zero Dependencies**: Only depends on TypeTraits.h and standard library
- **Header-Only**: No compilation or linking required
- **Zero Runtime Overhead**: All compile-time operations
- **Comprehensive Coverage**: All fat_penelope containers and utilities
- **C++20 Concepts**: Full concept support when available
- **Duck-Typed Detection**: Works with any compatible interface
- **DbC Integration**: Compile-time contract enforcement
- **Diagnostic Utilities**: Detailed type analysis

### Relationship to TypeTraits.h

FatPTypeTraits complements the general-purpose TypeTraits library:

- **TypeTraits.h**: General type properties (is_container, is_hashable, etc.)
- **FatPTypeTraits.h**: Library-specific types (is_small_vector, is_tensor, etc.)

See the [TypeTraits User Manual](TypeTraits_UserManual.md) for general type trait documentation.

---

## What Are Library-Specific Type Traits?

Library-specific type traits answer questions about fat_penelope components:

- Is this type a SmallVector?
- Is this type a Tensor or tensor-like?
- Is this container thread-safe?
- Does this type use small buffer optimization?
- Can this type be used with parallel algorithms?

These traits enable:

1. **Generic Algorithms**: Write code that works with any container but optimizes for specific types
2. **Static Dispatch**: Choose implementation at compile time based on type
3. **Template Constraints**: Require specific fat_penelope types
4. **Documentation**: Self-documenting type requirements
5. **Diagnostics**: Clear error messages when constraints fail

### Example Use Case

```cpp
#include "FatPTypeTraits.h"
#include "SmallVector.h"
#include <vector>

// Generic algorithm that optimizes for SmallVector
template <typename Container>
void process(Container& c) {
    if constexpr (fat_p::is_small_vector_v<Container>) {
        // SmallVector-specific optimization
        // Can assume inline storage for small sizes
        std::cout << "Optimized path for SmallVector\n";
    } else {
        // Generic path
        std::cout << "Generic container path\n";
    }
    
    for (auto& item : c) {
        // Process items
    }
}

int main() {
    fat_p::SmallVector<int, 16> sv = {1, 2, 3};
    std::vector<int> v = {4, 5, 6};
    
    process(sv);  // Uses SmallVector optimization
    process(v);   // Uses generic path
}
```

---

## Architecture

FatPTypeTraits uses a carefully designed architecture to avoid circular dependencies while providing comprehensive type detection.

### Design Principles

1. **Forward Declarations Only**
   - No component headers are included
   - All types are forward-declared
   - No compilation dependencies on library components

2. **Default to `std::false_type`**
   - Traits return false by default
   - Component headers specialize their traits
   - Safe for types not yet included

3. **Component Specialization**
   - Each component specializes its own traits
   - Specializations appear at end of component headers
   - Automatic registration

4. **Detection-Based Traits**
   - Duck-typed traits use SFINAE
   - Work with any compatible interface
   - Don't require explicit specialization

### Architecture Diagram

```
FatPTypeTraits.h
â”œâ”€â”€ Forward declarations (no includes)
â”‚   â”œâ”€â”€ SmallVector forward declaration
â”‚   â”œâ”€â”€ Tensor forward declaration
â”‚   â””â”€â”€ ... (all library types)
â”œâ”€â”€ Trait templates (default: false_type)
â”‚   â”œâ”€â”€ is_small_vector<T> : false_type
â”‚   â”œâ”€â”€ is_tensor<T> : false_type
â”‚   â””â”€â”€ ...
â””â”€â”€ Detection helpers (SFINAE-based)
    â”œâ”€â”€ is_expected_like (duck-typed)
    â”œâ”€â”€ is_tensor_like (duck-typed)
    â””â”€â”€ ...

Component Headers (e.g., SmallVector.h)
â”œâ”€â”€ Component definition
â””â”€â”€ Trait specialization
    â””â”€â”€ template<> is_small_vector<SmallVector<...>> : true_type
```

### Why This Design?

**Problem**: Circular dependencies
```cpp
// BAD: Creates circular dependency
// FatPTypeTraits.h
#include "SmallVector.h"  // Needs SmallVector definition

// SmallVector.h
#include "FatPTypeTraits.h"  // Needs traits for SFINAE
// Result: Circular dependency!
```

**Solution**: Forward declarations + late specialization
```cpp
// GOOD: No circular dependency
// FatPTypeTraits.h
template <typename T, size_t N, typename A> class SmallVector;  // Forward
template <typename T> struct is_small_vector : false_type {};    // Default

// SmallVector.h
#include "FatPTypeTraits.h"
// SmallVector definition...
template <typename T, size_t N, typename A>
struct is_small_vector<SmallVector<T, N, A>> : true_type {};  // Specialize
```

---

## Container Type Detection

These traits detect specific fat_penelope container types.

### SmallVector

```cpp
template <typename T>
struct is_small_vector : std::false_type {};

template <typename T>
inline constexpr bool is_small_vector_v = is_small_vector<T>::value;
```

**Example:**
```cpp
#include "FatPTypeTraits.h"
#include "SmallVector.h"

fat_p::SmallVector<int, 16> sv;
static_assert(fat_p::is_small_vector_v<decltype(sv)>);
static_assert(!fat_p::is_small_vector_v<std::vector<int>>);
```

### CircularBuffer

```cpp
template <typename T>
struct is_circular_buffer : std::false_type {};

template <typename T>
inline constexpr bool is_circular_buffer_v = is_circular_buffer<T>::value;
```

**Example:**
```cpp
fat_p::CircularBuffer<int, 10> cb;
static_assert(fat_p::is_circular_buffer_v<decltype(cb)>);
```

### FlatMap

```cpp
template <typename T>
struct is_flat_map : std::false_type {};

template <typename T>
inline constexpr bool is_flat_map_v = is_flat_map<T>::value;
```

**Example:**
```cpp
fat_p::FlatMap<int, std::string> fm;
static_assert(fat_p::is_flat_map_v<decltype(fm)>);
static_assert(!fat_p::is_flat_map_v<std::map<int, std::string>>);
```

### FlatSet

```cpp
template <typename T>
struct is_flat_set : std::false_type {};

template <typename T>
inline constexpr bool is_flat_set_v = is_flat_set<T>::value;
```

### SortedContainer

```cpp
template <typename T>
struct is_sorted_container : std::false_type {};

template <typename T>
inline constexpr bool is_sorted_container_v = is_sorted_container<T>::value;
```

### SparseSet

```cpp
template <typename T>
struct is_sparse_set : std::false_type {};

template <typename T>
inline constexpr bool is_sparse_set_v = is_sparse_set<T>::value;
```

### SlotMap

```cpp
template <typename T>
struct is_slot_map : std::false_type {};

template <typename T>
inline constexpr bool is_slot_map_v = is_slot_map<T>::value;
```

### Complete List of Container Traits

```cpp
namespace fat_p {
    template <typename T> struct is_small_vector;
    template <typename T> struct is_circular_buffer;
    template <typename T> struct is_flat_map;
    template <typename T> struct is_flat_set;
    template <typename T> struct is_sorted_container;
    template <typename T> struct is_sparse_set;
    template <typename T> struct is_slot_map;
    
    // All with _v convenience aliases
    template <typename T> inline constexpr bool is_small_vector_v = is_small_vector<T>::value;
    template <typename T> inline constexpr bool is_circular_buffer_v = is_circular_buffer<T>::value;
    template <typename T> inline constexpr bool is_flat_map_v = is_flat_map<T>::value;
    template <typename T> inline constexpr bool is_flat_set_v = is_flat_set<T>::value;
    template <typename T> inline constexpr bool is_sorted_container_v = is_sorted_container<T>::value;
    template <typename T> inline constexpr bool is_sparse_set_v = is_sparse_set<T>::value;
    template <typename T> inline constexpr bool is_slot_map_v = is_slot_map<T>::value;
}
```

---

## Tensor and Mathematical Types

Traits for mathematical and tensor types.

### Tensor

```cpp
template <typename T>
struct is_tensor : std::false_type {};

template <typename T>
inline constexpr bool is_tensor_v = is_tensor<T>::value;
```

**Example:**
```cpp
fat_p::Tensor<double> t({3, 4, 5});
static_assert(fat_p::is_tensor_v<decltype(t)>);
```

### FixedTensor

```cpp
template <typename T>
struct is_fixed_tensor : std::false_type {};

template <typename T>
inline constexpr bool is_fixed_tensor_v = is_fixed_tensor<T>::value;
```

**Example:**
```cpp
fat_p::FixedTensor<float, 3, 4, 5> ft;
static_assert(fat_p::is_fixed_tensor_v<decltype(ft)>);
```

### CSRMatrix

```cpp
template <typename T>
struct is_csr_matrix : std::false_type {};

template <typename T>
inline constexpr bool is_csr_matrix_v = is_csr_matrix<T>::value;
```

### SimdVector

```cpp
template <typename T>
struct is_simd_vector : std::false_type {};

template <typename T>
inline constexpr bool is_simd_vector_v = is_simd_vector<T>::value;
```

### Complete Tensor Traits

```cpp
namespace fat_p {
    template <typename T> struct is_tensor;
    template <typename T> struct is_fixed_tensor;
    template <typename T> struct is_csr_matrix;
    template <typename T> struct is_simd_vector;
    
    template <typename T> inline constexpr bool is_tensor_v = is_tensor<T>::value;
    template <typename T> inline constexpr bool is_fixed_tensor_v = is_fixed_tensor<T>::value;
    template <typename T> inline constexpr bool is_csr_matrix_v = is_csr_matrix<T>::value;
    template <typename T> inline constexpr bool is_simd_vector_v = is_simd_vector<T>::value;
}
```

---

## Concurrent Types

Traits for lock-free and concurrent data structures.

### LockFreeQueue

```cpp
template <typename T>
struct is_lock_free_queue : std::false_type {};

template <typename T>
inline constexpr bool is_lock_free_queue_v = is_lock_free_queue<T>::value;
```

### LockFreeRingBuffer

```cpp
template <typename T>
struct is_lock_free_ring_buffer : std::false_type {};

template <typename T>
inline constexpr bool is_lock_free_ring_buffer_v = is_lock_free_ring_buffer<T>::value;
```

### ThreadPool

```cpp
template <typename T>
struct is_thread_pool : std::false_type {};

template <typename T>
inline constexpr bool is_thread_pool_v = is_thread_pool<T>::value;
```

### AtomicReference

```cpp
template <typename T>
struct is_atomic_reference : std::false_type {};

template <typename T>
inline constexpr bool is_atomic_reference_v = is_atomic_reference<T>::value;
```

### Complete Concurrent Traits

```cpp
namespace fat_p {
    template <typename T> struct is_lock_free_queue;
    template <typename T> struct is_lock_free_ring_buffer;
    template <typename T> struct is_thread_pool;
    template <typename T> struct is_atomic_reference;
    
    template <typename T> inline constexpr bool is_lock_free_queue_v = is_lock_free_queue<T>::value;
    template <typename T> inline constexpr bool is_lock_free_ring_buffer_v = is_lock_free_ring_buffer<T>::value;
    template <typename T> inline constexpr bool is_thread_pool_v = is_thread_pool<T>::value;
    template <typename T> inline constexpr bool is_atomic_reference_v = is_atomic_reference<T>::value;
}
```

---

## Memory Management Types

Traits for memory management and allocation types.

### AlignedVector

```cpp
template <typename T>
struct is_aligned_vector : std::false_type {};

template <typename T>
inline constexpr bool is_aligned_vector_v = is_aligned_vector<T>::value;
```

### ObjectPool

```cpp
template <typename T>
struct is_object_pool : std::false_type {};

template <typename T>
inline constexpr bool is_object_pool_v = is_object_pool<T>::value;
```

### NumaAllocator Detection

```cpp
template <typename T>
struct has_numa_allocator : std::false_type {};

template <typename T>
inline constexpr bool has_numa_allocator_v = has_numa_allocator<T>::value;
```

---

## Utility Types

Traits for utility and wrapper types.

### Expected

```cpp
template <typename T>
struct is_expected : std::false_type {};

template <typename T>
inline constexpr bool is_expected_v = is_expected<T>::value;
```

**Example:**
```cpp
fat_p::Expected<int, std::string> result = get_value();
static_assert(fat_p::is_expected_v<decltype(result)>);
```

### StrongId

```cpp
template <typename T>
struct is_strong_id : std::false_type {};

template <typename T>
inline constexpr bool is_strong_id_v = is_strong_id<T>::value;
```

### ValueGuard

```cpp
template <typename T>
struct is_value_guard : std::false_type {};

template <typename T>
inline constexpr bool is_value_guard_v = is_value_guard<T>::value;
```

### ScopeGuard

```cpp
template <typename T>
struct is_scope_guard : std::false_type {};

template <typename T>
inline constexpr bool is_scope_guard_v = is_scope_guard<T>::value;
```

### Complete Utility Traits

```cpp
namespace fat_p {
    template <typename T> struct is_expected;
    template <typename T> struct is_strong_id;
    template <typename T> struct is_value_guard;
    template <typename T> struct is_scope_guard;
    
    template <typename T> inline constexpr bool is_expected_v = is_expected<T>::value;
    template <typename T> inline constexpr bool is_strong_id_v = is_strong_id<T>::value;
    template <typename T> inline constexpr bool is_value_guard_v = is_value_guard<T>::value;
    template <typename T> inline constexpr bool is_scope_guard_v = is_scope_guard<T>::value;
}
```

---

## Composite Library Traits

High-level traits that combine multiple checks.

### is_library_container

Detects any fat_penelope container type.

```cpp
template <typename T>
struct is_library_container;

template <typename T>
inline constexpr bool is_library_container_v = is_library_container<T>::value;
```

**Example:**
```cpp
fat_p::SmallVector<int, 16> sv;
fat_p::FlatMap<int, std::string> fm;
std::vector<int> vec;

static_assert(fat_p::is_library_container_v<decltype(sv)>);   // true
static_assert(fat_p::is_library_container_v<decltype(fm)>);   // true
static_assert(!fat_p::is_library_container_v<decltype(vec)>); // false
```

### is_concurrent_container

Detects thread-safe containers.

```cpp
template <typename T>
struct is_concurrent_container;

template <typename T>
inline constexpr bool is_concurrent_container_v = is_concurrent_container<T>::value;
```

### is_tensor_type

Detects any tensor-like type (Tensor, FixedTensor, CSRMatrix, SimdVector).

```cpp
template <typename T>
struct is_tensor_type;

template <typename T>
inline constexpr bool is_tensor_type_v = is_tensor_type<T>::value;
```

### is_small_buffer_optimized

Detects containers using small buffer optimization.

```cpp
template <typename T>
struct is_small_buffer_optimized;

template <typename T>
inline constexpr bool is_small_buffer_optimized_v = is_small_buffer_optimized<T>::value;
```

**Example:**
```cpp
// Optimize for SBO containers
template <typename Container>
void insert_many(Container& c, int count) {
    if constexpr (fat_p::is_small_buffer_optimized_v<Container>) {
        // For SBO containers, fill inline storage first
        // Avoid unnecessary heap allocations for small sizes
    }
    
    for (int i = 0; i < count; ++i) {
        c.push_back(typename Container::value_type{});
    }
}
```

### is_parallel_algorithm_compatible

Detects types compatible with parallel algorithms.

```cpp
template <typename T>
struct is_parallel_algorithm_compatible;

template <typename T>
inline constexpr bool is_parallel_algorithm_compatible_v = 
    is_parallel_algorithm_compatible<T>::value;
```

### is_cache_aware_type

Detects cache-aware types (aligned or SIMD).

```cpp
template <typename T>
struct is_cache_aware_type {
    static constexpr bool value = is_aligned_vector_v<T> || is_simd_vector_v<T>;
};

template <typename T>
inline constexpr bool is_cache_aware_type_v = is_cache_aware_type<T>::value;
```

---

## Duck-Typed Detection

These traits use duck-typing to detect interfaces rather than specific types. They work with any type implementing the required interface.

### is_expected_like

Detects types with Expected-like interface (value(), error(), error_type).

```cpp
template <typename T>
struct is_expected_like;

template <typename T>
inline constexpr bool is_expected_like_v = is_expected_like<T>::value;
```

**Example:**
```cpp
// Works with fat_p::Expected
fat_p::Expected<int, std::string> e1 = 42;
static_assert(fat_p::is_expected_like_v<decltype(e1)>);

// Also works with custom Expected-like types
struct MyResult {
    int value() const;
    std::string error() const;
    using error_type = std::string;
};

static_assert(fat_p::is_expected_like_v<MyResult>);  // Also true!
```

### is_tensor_like

Detects types with Tensor-like interface (shape(), data(), etc.).

```cpp
template <typename T>
struct is_tensor_like;

template <typename T>
inline constexpr bool is_tensor_like_v = is_tensor_like<T>::value;
```

**Example:**
```cpp
// Works with fat_p::Tensor
fat_p::Tensor<float> t({3, 4});
static_assert(fat_p::is_tensor_like_v<decltype(t)>);

// Also works with custom tensor-like types
struct MyTensor {
    auto shape() const -> std::vector<size_t>;
    float* data();
    // ... other tensor operations
};

static_assert(fat_p::is_tensor_like_v<MyTensor>);  // Also true!
```

### is_binary_serializable

Detects types with binary serialization support.

```cpp
template <typename T>
struct is_binary_serializable;

template <typename T>
inline constexpr bool is_binary_serializable_v = is_binary_serializable<T>::value;
```

**Example:**
```cpp
struct Serializable {
    void binary_serialize(std::vector<uint8_t>& buffer) const;
    static Serializable binary_deserialize(const std::vector<uint8_t>& buffer);
};

static_assert(fat_p::is_binary_serializable_v<Serializable>);
```

---

## Policy Detection

Traits for detecting policy types and their capabilities.

### has_validate

Detects if a policy has a validate() method.

```cpp
template <typename T>
struct has_validate;

template <typename T>
inline constexpr bool has_validate_v = has_validate<T>::value;
```

### has_shared_locking

Detects if a concurrency policy supports shared locking.

```cpp
template <typename T>
struct has_shared_locking;

template <typename T>
inline constexpr bool has_shared_locking_v = has_shared_locking<T>::value;
```

### is_lock_free_policy

Detects if a policy is lock-free.

```cpp
template <typename T>
struct is_lock_free_policy;

template <typename T>
inline constexpr bool is_lock_free_policy_v = is_lock_free_policy<T>::value;
```

---

## Diagnostic Utilities

FatPTypeTraits provides diagnostic functions for understanding type properties. All diagnostic functions return compile-time string literals with **zero runtime overhead** - no heap allocations, no string construction, just a pointer to a static string.

### diagnose_expected

Provides a diagnostic message about Expected-like types. Returns a compile-time string literal with zero runtime overhead.

```cpp
template <typename T>
constexpr const char* diagnose_expected();
```

**Example:**
```cpp
#include "FatPTypeTraits.h"
#include <iostream>

struct MyType {
    int data;
};

int main() {
    const char* diag = fat_p::diagnose_expected<MyType>();
    std::cout << diag << "\n";
    // Output: "Missing value() method"
}
```

### diagnose_tensor

Provides a diagnostic message about Tensor types. Returns a compile-time string literal with zero runtime overhead.

```cpp
template <typename T>
constexpr const char* diagnose_tensor();
```

**Example:**
```cpp
const char* diag = fat_p::diagnose_tensor<MyTensor>();
std::cout << diag << "\n";
// Example outputs:
// "Type is specialized as Tensor"
// "Type is specialized as CSRMatrix"
// "Missing shape() method"
```

### diagnose_binary_serializable

Analyzes binary serialization capabilities. Returns a compile-time string literal with zero runtime overhead.

```cpp
template <typename T>
constexpr const char* diagnose_binary_serializable();
```

**Example:**
```cpp
const char* diag = fat_p::diagnose_binary_serializable<MyType>();
std::cout << diag << "\n";
// Example outputs:
// "Type is fully binary serializable"
// "Missing binary_serialize() method"
// "Missing binary_deserialize() static method"
```

### diagnose_library_container

Analyzes container classification. Returns a compile-time string literal with zero runtime overhead.

```cpp
template <typename T>
constexpr const char* diagnose_library_container();
```

**Example:**
```cpp
const char* diag = fat_p::diagnose_library_container<MyContainer>();
std::cout << diag << "\n";
// Example outputs:
// "Type is SmallVector"
// "Type is FlatMap"
// "Type is a concurrent container"
// "Type is not a library container"
```

### why_not_* Helpers

```cpp
template <typename T>
struct why_not_expected {
    static constexpr const char* reason;
};

template <typename T>
struct why_not_tensor {
    static constexpr const char* reason;
};

template <typename T>
struct why_not_binary_serializable {
    static constexpr const char* reason;
};
```

**Example:**
```cpp
struct Incomplete {
    int value() const { return 42; }
    // Missing error() and error_type
};

std::cout << fat_p::why_not_expected<Incomplete>::reason << "\n";
// Output: "Missing error() method"
```

---

## Design-by-Contract Helpers

DbC helpers provide compile-time contract enforcement for library types.

### requires_tensor

```cpp
template<typename T>
constexpr void requires_tensor() {
    static_assert(is_tensor_v<T>,
        "[CONTRACT VIOLATION] Type must be fat_p::Tensor");
}
```

**Example:**
```cpp
template <typename T>
void process_tensor(T& tensor) {
    fat_p::requires_tensor<T>();  // Compile error if not a Tensor
    // Work with tensor
}
```

### requires_parallel_compatible

```cpp
template<typename T>
constexpr void requires_parallel_compatible() {
    static_assert(is_parallel_algorithm_compatible_v<T>,
        "[CONTRACT VIOLATION] Type must support parallel algorithms");
}
```

### requires_binary_serializable

```cpp
template<typename T>
constexpr void requires_binary_serializable() {
    static_assert(is_binary_serializable_v<T>,
        "[CONTRACT VIOLATION] Type must support binary serialization");
}
```

### requires_library_container

```cpp
template<typename T>
constexpr void requires_library_container() {
    static_assert(is_library_container_v<T>,
        "[CONTRACT VIOLATION] Type must be a fat_p container");
}
```

### requires_concurrent_container

```cpp
template<typename T>
constexpr void requires_concurrent_container() {
    static_assert(is_concurrent_container_v<T>,
        "[CONTRACT VIOLATION] Type must be a concurrent container");
}
```

### requires_tensor_type

```cpp
template<typename T>
constexpr void requires_tensor_type() {
    static_assert(is_tensor_type_v<T>,
        "[CONTRACT VIOLATION] Type must be a tensor type");
}
```

### Complete DbC Example

```cpp
#include "FatPTypeTraits.h"
#include "Tensor.h"
#include "SmallVector.h"

template <typename TensorType>
void process_matrix_multiply(const TensorType& a, const TensorType& b) {
    // Enforce tensor requirement at compile time
    fat_p::requires_tensor<TensorType>();
    
    // Ensure dimensions match
    assert(a.shape()[1] == b.shape()[0]);
    
    // Implementation...
}

template <typename Container>
void parallel_sort(Container& c) {
    // Ensure container supports parallel algorithms
    fat_p::requires_parallel_compatible<Container>();
    fat_p::requires_library_container<Container>();
    
    // Use parallel algorithm
    std::sort(std::execution::par, c.begin(), c.end());
}
```

---

## C++20 Concepts

When C++20 is available, FatPTypeTraits provides native concept definitions.

### Container Concepts

```cpp
#if __cplusplus >= 202002L
namespace fat_p {
    template <typename T>
    concept SmallVector = is_small_vector_v<T>;
    
    template <typename T>
    concept CircularBuffer = is_circular_buffer_v<T>;
    
    template <typename T>
    concept FlatMap = is_flat_map_v<T>;
    
    template <typename T>
    concept FlatSet = is_flat_set_v<T>;
    
    template <typename T>
    concept LibraryContainer = is_library_container_v<T>;
}
#endif
```

### Tensor Concepts

```cpp
#if __cplusplus >= 202002L
namespace fat_p {
    template <typename T>
    concept Tensor = is_tensor_v<T>;
    
    template <typename T>
    concept FixedTensor = is_fixed_tensor_v<T>;
    
    template <typename T>
    concept TensorType = is_tensor_type_v<T>;
}
#endif
```

### Utility Concepts

```cpp
#if __cplusplus >= 202002L
namespace fat_p {
    template <typename T>
    concept Expected = is_expected_v<T>;
    
    template <typename T>
    concept StrongId = is_strong_id_v<T>;
    
    template <typename T>
    concept BinarySerializable = is_binary_serializable_v<T>;
    
    template <typename T>
    concept ParallelCompatible = is_parallel_algorithm_compatible_v<T>;
    
    template <typename T>
    concept ConcurrentContainer = is_concurrent_container_v<T>;
}
#endif
```

### Usage Examples (C++20)

```cpp
#include "FatPTypeTraits.h"
#include "SmallVector.h"
#include "Tensor.h"

// Direct concept usage
template <fat_p::SmallVector SV>
void process_small_vector(SV& sv) {
    // Guaranteed to be a SmallVector
}

// Concept in requires clause
template <typename T>
    requires fat_p::Tensor<T>
void matrix_multiply(const T& a, const T& b) {
    // Tensor operations
}

// Abbreviated function template
void display(fat_p::LibraryContainer auto const& container) {
    for (const auto& item : container) {
        std::cout << item << " ";
    }
}

// Concept composition
template <typename T>
    requires fat_p::LibraryContainer<T> && fat_p::BinarySerializable<T>
void save_container(const T& container, const std::string& filename) {
    // Serialize and save
}
```

---

## Integration with TypeTraits

FatPTypeTraits is designed to work seamlessly with the general TypeTraits library.

### Complementary Usage

```cpp
#include "TypeTraits.h"
#include "FatPTypeTraits.h"
#include "SmallVector.h"
#include <vector>

template <typename Container>
void advanced_process(Container& c) {
    // General trait from TypeTraits.h
    static_assert(fat_p::is_container_v<Container>, 
        "Must be a container");
    
    // Library-specific trait from FatPTypeTraits.h
    if constexpr (fat_p::is_small_vector_v<Container>) {
        std::cout << "SmallVector optimization enabled\n";
    }
    
    // Check element type with general trait
    using value_t = typename Container::value_type;
    if constexpr (fat_p::is_hashable_v<value_t>) {
        // Can use hash-based algorithms
    }
}
```

### Layered Checking

```cpp
#include "TypeTraits.h"
#include "FatPTypeTraits.h"

template <typename T>
void process(T& obj) {
    // Layer 1: General capability (TypeTraits)
    if constexpr (fat_p::is_container_v<T>) {
        std::cout << "Is a container\n";
        
        // Layer 2: Specific optimization (FatPTypeTraits)
        if constexpr (fat_p::is_small_buffer_optimized_v<T>) {
            std::cout << "Uses SBO - can optimize for small sizes\n";
        }
        
        // Layer 3: Element properties (TypeTraits)
        using value_t = typename T::value_type;
        if constexpr (fat_p::is_trivially_copyable_v<value_t>) {
            std::cout << "Elements are trivially copyable - can use memcpy\n";
        }
    }
}
```

### Combining Constraints

```cpp
#include "TypeTraits.h"
#include "FatPTypeTraits.h"

// C++17: SFINAE
template <typename Container>
std::enable_if_t<
    fat_p::is_container_v<Container> &&          // General: is container
    fat_p::is_library_container_v<Container> &&  // Specific: library type
    fat_p::is_reservable_v<Container>,           // General: has reserve()
    void
>
optimized_insert(Container& c, size_t count) {
    c.reserve(c.size() + count);
    // Bulk insert
}

// C++20: Concepts
template <typename Container>
    requires fat_p::is_container<Container> &&
             fat_p::LibraryContainer<Container> &&
             fat_p::is_reservable_v<Container>
void optimized_insert_cpp20(Container& c, size_t count) {
    c.reserve(c.size() + count);
    // Bulk insert
}
```

---

## Usage Examples

### Example 1: Generic Algorithm with Library Optimization

```cpp
#include "TypeTraits.h"
#include "FatPTypeTraits.h"
#include "SmallVector.h"
#include "CircularBuffer.h"
#include <vector>

template <typename Container>
void display(const Container& c) {
    // Ensure it's a container using general trait
    fat_p::requires_container<Container>();
    
    std::cout << "Size: " << c.size() << "\n";
    
    // Optimize based on specific type
    if constexpr (fat_p::is_small_vector_v<Container>) {
        std::cout << "SmallVector: inline storage optimization\n";
    } else if constexpr (fat_p::is_circular_buffer_v<Container>) {
        std::cout << "CircularBuffer: fixed-size optimization\n";
    } else {
        std::cout << "Standard container\n";
    }
    
    // Check if we can use library-wide optimizations
    if constexpr (fat_p::is_library_container_v<Container>) {
        std::cout << "Using fat_p container optimizations\n";
    }
    
    for (const auto& item : c) {
        std::cout << item << " ";
    }
    std::cout << "\n";
}

int main() {
    std::vector<int> std_vec = {1, 2, 3};
    fat_p::SmallVector<int, 16> small_vec = {4, 5, 6};
    fat_p::CircularBuffer<int, 10> circular = {7, 8, 9};
    
    display(std_vec);      // Standard container
    display(small_vec);    // SmallVector optimization
    display(circular);     // CircularBuffer optimization
}
```

### Example 2: Type-Specific Dispatch

```cpp
#include "FatPTypeTraits.h"
#include "Tensor.h"
#include "FixedTensor.h"

namespace detail {
    // Dynamic tensor path
    template <typename T>
    void compute_impl(T& tensor, std::false_type) {
        std::cout << "Dynamic tensor computation\n";
        // Use runtime shape information
        for (size_t i = 0; i < tensor.size(); ++i) {
            tensor.data()[i] *= 2;
        }
    }
    
    // Fixed tensor path - compile-time optimization
    template <typename T>
    void compute_impl(T& tensor, std::true_type) {
        std::cout << "Fixed tensor computation (optimized)\n";
        // Compile-time unrolling possible
        constexpr size_t Size = T::total_size;
        #pragma unroll
        for (size_t i = 0; i < Size; ++i) {
            tensor.data()[i] *= 2;
        }
    }
}

template <typename TensorType>
void compute(TensorType& tensor) {
    fat_p::requires_tensor_type<TensorType>();
    
    // Dispatch based on fixed vs dynamic
    detail::compute_impl(tensor, 
        std::bool_constant<fat_p::is_fixed_tensor_v<TensorType>>{});
}
```

### Example 3: Serialization Framework

```cpp
#include "FatPTypeTraits.h"
#include "BinarySerializer.h"

template <typename T>
void save(const T& obj, std::vector<uint8_t>& buffer) {
    if constexpr (fat_p::is_binary_serializable_v<T>) {
        // Use library's binary serialization
        obj.binary_serialize(buffer);
    } else if constexpr (fat_p::is_tensor_v<T>) {
        // Custom tensor serialization
        // Serialize shape
        const auto& shape = obj.shape();
        buffer.insert(buffer.end(), 
            reinterpret_cast<const uint8_t*>(shape.data()),
            reinterpret_cast<const uint8_t*>(shape.data() + shape.size()));
        
        // Serialize data
        buffer.insert(buffer.end(),
            reinterpret_cast<const uint8_t*>(obj.data()),
            reinterpret_cast<const uint8_t*>(obj.data() + obj.size()));
    } else {
        static_assert(fat_p::is_binary_serializable_v<T>,
            "Type must be serializable. Use why_not_binary_serializable<T>::reason");
    }
}
```

### Example 4: Parallel Algorithm Selection

```cpp
#include "FatPTypeTraits.h"
#include <execution>
#include <algorithm>

template <typename Container>
void parallel_sort(Container& c) {
    if constexpr (fat_p::is_parallel_algorithm_compatible_v<Container>) {
        // Use parallel execution
        std::sort(std::execution::par, c.begin(), c.end());
    } else {
        // Fallback to sequential
        std::sort(c.begin(), c.end());
    }
}
```

---

## Best Practices

### 1. Always Check General Traits First

```cpp
// Good: Check general trait, then specialize
template <typename T>
void process(T& obj) {
    // First: general capability
    if constexpr (fat_p::is_container_v<T>) {
        // Then: specific optimization
        if constexpr (fat_p::is_small_vector_v<T>) {
            // SmallVector-specific code
        }
    }
}

// Avoid: Checking library trait without general check
template <typename T>
void process_bad(T& obj) {
    if constexpr (fat_p::is_small_vector_v<T>) {
        // Missing check for is_container_v
        obj.size();  // Might not compile if T isn't a container
    }
}
```

### 2. Use Composite Traits for Common Patterns

```cpp
// Good: Use composite trait
template <typename T>
void bulk_insert(T& container) {
    if constexpr (fat_p::is_small_buffer_optimized_v<T>) {
        // Single check covers SmallVector, FlatMap, FlatSet
    }
}

// Avoid: Checking each type individually
template <typename T>
void bulk_insert_bad(T& container) {
    if constexpr (fat_p::is_small_vector_v<T> ||
                  fat_p::is_flat_map_v<T> ||
                  fat_p::is_flat_set_v<T>) {
        // More verbose, harder to maintain
    }
}
```

### 3. Use DbC Helpers for Clear Errors

```cpp
// Good: Clear error message
template <typename T>
void tensor_operation(T& tensor) {
    fat_p::requires_tensor<T>();  // Clear: "[CONTRACT VIOLATION] Type must be fat_p::Tensor"
    // Implementation
}

// Avoid: Generic assertion
template <typename T>
void tensor_operation_bad(T& tensor) {
    static_assert(fat_p::is_tensor_v<T>, "Wrong type");  // Less helpful
}
```

### 4. Leverage Duck-Typed Traits

```cpp
// Good: Works with any compatible interface
template <typename ExpectedLike>
void handle_result(ExpectedLike& result) {
    if constexpr (fat_p::is_expected_like_v<ExpectedLike>) {
        if (result.has_value()) {
            use(result.value());
        } else {
            handle_error(result.error());
        }
    }
}

// This works with:
// - fat_p::Expected<T, E>
// - std::expected<T, E> (C++23)
// - Custom Expected-like types
```

### 5. Use Diagnostics During Development

```cpp
// During development
#include "FatPTypeTraits.h"
#include <iostream>

template <typename T>
void debug_type() {
    std::cout << fat_p::diagnose_library_container<T>() << "\n";
    std::cout << fat_p::diagnose_tensor<T>() << "\n";
    std::cout << fat_p::diagnose_binary_serializable<T>() << "\n";
}

// In production: Remove or wrap in #ifdef DEBUG
```

### 6. Document Type Requirements

```cpp
/**
 * @brief Processes a fat_penelope container
 * @tparam Container Must satisfy:
 *         - fat_p::is_library_container_v<Container>
 *         - fat_p::is_reservable_v<Container>
 * @param c The container to process
 */
template <typename Container>
void process_library_container(Container& c) {
    fat_p::requires_library_container<Container>();
    fat_p::requires_reservable<Container>();
    // Implementation
}
```

### 7. Prefer Concepts in C++20

```cpp
// C++17: SFINAE
template <typename T>
std::enable_if_t<fat_p::is_tensor_v<T>, void>
compute_cpp17(T& tensor) { }

// C++20: Concepts (preferred)
template <fat_p::Tensor T>
void compute_cpp20(T& tensor) { }

// Or with requires
template <typename T>
    requires fat_p::Tensor<T>
void compute_cpp20_alt(T& tensor) { }
```

### 8. Combine with Standard Traits

```cpp
#include <type_traits>
#include "FatPTypeTraits.h"

template <typename T>
void optimized_copy(const T& src, T& dst) {
    if constexpr (std::is_trivially_copyable_v<T> &&
                  fat_p::is_contiguous_container_v<T>) {
        // Use memcpy for trivially copyable contiguous containers
        std::memcpy(dst.data(), src.data(), src.size() * sizeof(typename T::value_type));
    } else {
        // Element-wise copy
        dst = src;
    }
}
```

---

## Performance Considerations

### Compile-Time Only

All FatPTypeTraits operations are compile-time only:

```cpp
// Zero runtime cost
if constexpr (fat_p::is_small_vector_v<T>) {
    // Branch selected at compile time
}

// No runtime overhead
template <typename T>
void process(T& obj) {
    fat_p::requires_tensor<T>();  // Compile-time only
}
```

### Forward Declaration Benefits

The forward declaration design keeps compilation fast:

```cpp
// FatPTypeTraits.h is very lightweight
// - No component includes
// - Just forward declarations and trait templates
// - Fast to parse

// Only pay for what you use
#include "FatPTypeTraits.h"  // Lightweight
#include "SmallVector.h"      // Only if needed
```

### Trait Caching

For complex queries, cache results:

```cpp
template <typename Container>
class Processor {
    // Cache complex trait combination
    static constexpr bool can_optimize = 
        fat_p::is_library_container_v<Container> &&
        fat_p::is_small_buffer_optimized_v<Container> &&
        fat_p::is_reservable_v<Container>;
    
public:
    void process(Container& c) {
        if constexpr (can_optimize) {
            // Use cached result
        }
    }
};
```

### Diagnostic Functions

Diagnostic functions have minimal overhead but should be used primarily in development:

```cpp
// Development: Use diagnostics
#ifdef DEBUG
    std::cout << fat_p::diagnose_library_container<T>() << "\n";
#endif

// Production: Traits only (zero runtime cost)
if constexpr (fat_p::is_library_container_v<T>) {
    // Optimized path
}
```

---

## Conclusion

FatPTypeTraits provides comprehensive compile-time type detection for fat_penelope library components. By using these traits, you can:

1. **Write Generic Code**: Algorithms that work with any container
2. **Enable Optimizations**: Detect specific types for specialized paths
3. **Enforce Contracts**: Compile-time validation of type requirements
4. **Improve Diagnostics**: Clear error messages when constraints fail
5. **Support Duck-Typing**: Work with any compatible interface
6. **Zero Runtime Cost**: All detection happens at compile time

### Key Takeaways

- **Complementary to TypeTraits.h**: Use together for best results
- **Forward Declaration Design**: No circular dependencies
- **C++20 Concepts**: Full support when available
- **Comprehensive Coverage**: All fat_penelope types detected
- **Duck-Typed Detection**: Works beyond library types
- **Diagnostic Utilities**: Development-friendly error analysis

### Related Documentation

- **[TypeTraits User Manual](TypeTraits_UserManual.md)**: General-purpose type traits
- **Component Documentation**: Specific container/utility docs

### Quick Reference

```cpp
// Include both for complete functionality
#include "TypeTraits.h"        // General traits
#include "FatPTypeTraits.h"    // Library-specific traits

// Container detection
is_small_vector_v, is_circular_buffer_v, is_flat_map_v, etc.

// Tensor detection
is_tensor_v, is_fixed_tensor_v, is_csr_matrix_v, is_simd_vector_v

// Concurrent detection
is_lock_free_queue_v, is_thread_pool_v, is_concurrent_container_v

// Composite traits
is_library_container_v, is_tensor_type_v, is_small_buffer_optimized_v

// Duck-typed
is_expected_like_v, is_tensor_like_v, is_binary_serializable_v

// Diagnostics
diagnose_expected<T>(), diagnose_tensor<T>(), why_not_expected<T>::reason

// DbC
requires_tensor<T>(), requires_library_container<T>()
```

Happy metaprogramming with fat_penelope!
