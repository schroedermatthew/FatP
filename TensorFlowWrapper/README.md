# TensorFlow C++20 Wrapper - Merged Implementation

A modern, type-safe, thread-safe C++20 wrapper for the TensorFlow C API.

## Overview

This is the **merged "best of both worlds"** implementation combining:
- **ChatGPT's approach**: Correctness-first, verification-driven
- **Claude's approach**: Comprehensive feature set, detailed documentation

## Features

- 🔒 **Policy-based thread safety** (NoLock, Mutex, SharedMutex)
- 🛡️ **RAII everywhere** - no resource leaks
- 📐 **Guarded views** - thread-safe data access
- 🚀 **Zero-overhead abstraction** when thread safety not needed
- 📝 **Modern C++20** - concepts, `std::span`, `std::format`, `source_location`

## Installation

```bash
# Extract
tar -xzf tf_wrapper_merged.tar.gz
cd tf_wrapper_merged

# Configure
mkdir build && cd build
cmake .. -DTF_ROOT=/path/to/tensorflow

# Build example
cmake --build . -j$(nproc)

# Run
./tf_example
```

## Quick Start

```cpp
#include <tf/all.hpp>

int main() {
    // Build graph
    tf::Graph<> graph;
    
    auto tensor = tf::Tensor<>::FromVector<float>({1, 4}, {1, 2, 3, 4});
    
    auto const_op = std::move(graph.NewOperation("Const", "data"))
        .SetAttrTensor("value", tensor.handle())
        .SetAttrType("dtype", TF_FLOAT)
        .Finish();
    
    std::move(graph.NewOperation("Identity", "output"))
        .AddInput(const_op, 0)
        .Finish();
    
    // Thread-safe session
    tf::Session<tf::policy::Mutex> session(graph);
    auto results = session.Run({tf::Fetch{"output", 0}});
    
    // Safe data access
    auto view = results[0].read<float>();
    for (float x : view) {
        std::cout << x << " ";
    }
}
```

## Thread-Safe Data Access

```cpp
// View-based (lock held for view lifetime)
{
    auto view = tensor.read<float>();   // Shared lock
    for (float x : view) process(x);
}  // Lock released

{
    auto view = tensor.write<float>();  // Exclusive lock
    view[0] = 42.0f;
}  // Lock released

// Callback-based (hardest to misuse)
tensor.with_read<float>([](std::span<const float> s) {
    // Lock held here
});

// Unsafe (NO lock - caller must synchronize)
float* p = tensor.unsafe_data<float>();  // Dangerous!
```

## Feed Tensor Locking (Advanced)

When running inference with feed tensors, ensure feeds aren't mutated during execution:

```cpp
Tensor<Mutex> input = ...;

// Option 1: Hold read guard manually (recommended)
{
    auto guard = input.acquire_shared_lock();  // Lock acquired
    session.Run({Feed{"input", input}}, {Fetch{"output"}});
}  // Lock released

// Option 2: Use read view (also holds lock)
{
    auto view = input.read<float>();  // Lock acquired
    session.Run({Feed{"input", input}}, {Fetch{"output"}});
}  // Lock released

// For multiple feeds, acquire locks in consistent order to avoid deadlock:
auto lock_a = tensor_a.acquire_shared_lock();
auto lock_b = tensor_b.acquire_shared_lock();  // Always same order!
session.Run({Feed{"a", tensor_a}, Feed{"b", tensor_b}}, ...);
```

## All Fixed Issues

### P0: Won't Compile / Undefined Behavior

| Issue | Fix |
|-------|-----|
| `adopt_lock` UB | Policies return `unique_lock`/`shared_lock` directly |
| Guard lifetime bug | Views hold lock for entire lifetime |
| `TF_Status` leak | RAII `Status` class with `reset()` |
| `FromRaw` invalid requires | Runtime check instead of compile-time |
| `TF_TensorDims` non-existent | Uses `TF_NumDims` + `TF_Dim` loop |
| Missing includes | All headers self-contained |
| `SessionOptions` undefined | Full RAII wrapper |
| `always_false` undefined | Defined in `detail` namespace |

### P1: Compiles But Wrong

| Issue | Fix |
|-------|-----|
| `noexcept` on throwing functions | Removed |
| Concept mismatch | `Guard` concept, matching implementations |
| `std::mutex` not movable | `shared_ptr<mutex>` storage |
| Pointer escapes lock | Guarded view API |

### P2: Design Flaws

| Issue | Fix |
|-------|-----|
| `Session` hardcodes `Graph<>` | Templated constructor |
| `OperationBuilder` no lock | Holds lock until `Finish()` |
| Example doesn't compile | Correct examples |
| Missing headers in docs | Only document what exists |
| Feed tensor races | `acquire_shared_lock()` API + documentation |

### P3: Enhancements

| Enhancement | Status |
|-------------|--------|
| `[[nodiscard]]` on factories | ✅ Added |
| `Status::reset()` for reuse | ✅ Added |
| 11 scalar types | ✅ Added |
| Type aliases | ✅ Added |
| `GuardedSpan::at()` | ✅ Added |
| `Tensor::Zeros` factory | ✅ Added |
| `dtype_name()` function | ✅ Added |
| `Status::code_name()` | ✅ Added |

## Thread Safety Policies

| Policy | Behavior | Use Case |
|--------|----------|----------|
| `NoLock` | No synchronization | Single-threaded, maximum performance |
| `Mutex` | Exclusive locking | Multi-threaded writes |
| `SharedMutex` | Reader-writer locks | Many readers, few writers |

## Type Aliases

```cpp
// Tensors
tf::FastTensor   = tf::Tensor<tf::policy::NoLock>
tf::SafeTensor   = tf::Tensor<tf::policy::Mutex>
tf::SharedTensor = tf::Tensor<tf::policy::SharedMutex>

// Graphs
tf::FastGraph    = tf::Graph<tf::policy::NoLock>
tf::SafeGraph    = tf::Graph<tf::policy::Mutex>
tf::SharedGraph  = tf::Graph<tf::policy::SharedMutex>

// Sessions
tf::FastSession  = tf::Session<tf::policy::NoLock>
tf::SafeSession  = tf::Session<tf::policy::Mutex>
```

## Supported Scalar Types

- `float`, `double`
- `int8_t`, `int16_t`, `int32_t`, `int64_t`
- `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`
- `bool`

## Building with Tests

```bash
cmake .. -DTF_WRAPPER_BUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```

## API Summary

### Tensor

```cpp
// Factories
Tensor::FromVector<T>(dims, data)  // Copy from vector
Tensor::FromScalar<T>(value)       // Single element
Tensor::FromRaw(TF_Tensor*)        // Adopt ownership
Tensor::Allocate<T>(dims)          // Uninitialized
Tensor::Zeros<T>(dims)             // Zero-initialized

// Safe access
ReadView<T> read<T>() const        // Shared lock
WriteView<T> write<T>()            // Exclusive lock
with_read<T>(callback)             // Callback with shared lock
with_write<T>(callback)            // Callback with exclusive lock

// Lock acquisition (for feed tensors, etc.)
acquire_shared_lock()              // Get shared guard without data
acquire_exclusive_lock()           // Get exclusive guard without data

// Unsafe access
T* unsafe_data<T>()                // No lock!

// Queries
shape(), rank(), dtype(), dtype_name()
num_elements(), byte_size(), empty()
```

### Graph

```cpp
Graph()                                      // Create empty graph
ImportGraphDef(proto, len, prefix)           // Import serialized graph
GetOperation(name) -> optional<TF_Operation*>
GetOperationOrThrow(name) -> TF_Operation*
HasOperation(name) -> bool
NewOperation(type, name) -> OperationBuilder // Holds lock!
GetAllOperations() -> vector<TF_Operation*>
```

### Session

```cpp
Session(graph, options)                           // Any graph policy
Run(feeds, fetches, targets) -> vector<Tensor<>>
Run(fetches) -> vector<Tensor<>>
Run(fetch_name, index) -> Tensor<>
```

### Status

```cpp
Status()                    // Create OK status
ok(), code(), message()     // Inspection
code_name()                 // Human-readable code
reset()                     // Reset to OK (for reuse)
throw_if_error(context)     // Throw if not OK
```

## License

MIT

## Credits

- **ChatGPT**: Correctness verification, `shared_ptr` mutex pattern, helper methods
- **Claude**: Comprehensive features, STL compatibility, documentation
- **Original Author**: Initial wrapper design
