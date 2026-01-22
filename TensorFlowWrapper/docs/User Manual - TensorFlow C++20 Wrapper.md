---
doc_id: UM-TFWRAPPER-001
doc_type: "User Manual"
title: "TensorFlow C++20 Wrapper"
fatp_components: ["tf/all", "tf/tensor", "tf/graph", "tf/session", "tf/status", "tf/policy"]
topics: ["TensorFlow C API wrapping", "RAII resource management", "policy-based thread safety", "tensor memory safety", "graph building", "session execution", "SavedModel loading", "device enumeration"]
constraints: ["TensorFlow C API lifetime rules", "thread-safe tensor access", "lock guard lifetime", "exception safety", "moved-from object safety", "graph must outlive session"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: null
build_modes: ["Debug", "Release"]
last_verified: "2025-01-21"
audience: ["C++ developers", "ML engineers", "AI assistants"]
status: "reviewed"
---

# User Manual - TensorFlow C++20 Wrapper

## Scope

This manual covers correct usage of the TensorFlow C++20 Wrapper library, including tensor creation, thread-safe data access, graph building, session execution, and error handling. After reading this manual, you will understand when to use each thread-safety policy, how to avoid common lifetime pitfalls, and how to integrate the library into production code.

## Not covered

- TensorFlow model training (use Python for training, this library for inference)
- TensorFlow Lite or TensorFlow.js integration
- Performance tuning of TensorFlow itself
- Explicit device placement (device listing is supported, but placement requires raw C API)

## Prerequisites

- C++20 compiler (GCC 11+, Clang 13+, MSVC 2022+)
- TensorFlow C library installed (libtensorflow 2.x)
- Familiarity with RAII and move semantics
- Basic understanding of TensorFlow computational graphs

---

## User Manual Card

**Component:** TensorFlow C++20 Wrapper  
**Primary use case:** Type-safe, RAII-managed TensorFlow inference in C++ applications  
**Integration pattern:** Header-only; include `<tf/all.hpp>` and link against TensorFlow C library  
**Key API:** `tf::Tensor<Policy>`, `tf::Graph<Policy>`, `tf::Session<Policy>`, `tf::Status`, `tf::Device`  
**std equivalent:** None. The TensorFlow C++ API (`tensorflow/cc`) exists but requires building TensorFlow from source  
**Migration from std:** N/A — no standard equivalent  
**Common mistakes:** Holding tensor views across scope boundaries, forgetting to call `Finish()` on operation builders, using wrong dtype template parameter, destroying graph before session  
**Performance notes:** `NoLock` policy has zero overhead; `SharedMutex` enables concurrent reads; view-based access avoids copies; `HasGPU()` enables runtime hardware detection

---

## Table of Contents

- [The TensorFlow C API Problem](#the-tensorflow-c-api-problem)
- [Architecture: How the Wrapper Works](#architecture-how-the-wrapper-works)
- [Quick Start](#quick-start)
- [Thread Safety: Three Policies, Three Tradeoffs](#thread-safety-three-policies-three-tradeoffs)
- [Tensor Creation: Five Factories, Five Philosophies](#tensor-creation-five-factories-five-philosophies)
- [Tensor Data Access: Views, Callbacks, and Unsafe Pointers](#tensor-data-access-views-callbacks-and-unsafe-pointers)
- [Graph Building: The Operation Builder Pattern](#graph-building-the-operation-builder-pattern)
- [Session Execution: Feeds, Fetches, and Targets](#session-execution-feeds-fetches-and-targets)
- [Loading SavedModels](#loading-savedmodels)
- [Device Enumeration](#device-enumeration)
- [Error Handling Model](#error-handling-model)
- [Performance Rules of Thumb](#performance-rules-of-thumb)
- [When to Use This Library (and When Not To)](#when-to-use-this-library-and-when-not-to)
- [Migration from Raw TensorFlow C API](#migration-from-raw-tensorflow-c-api)
- [Troubleshooting](#troubleshooting)
- [API Reference](#api-reference)
- [FAQ](#faq)
- [Glossary](#glossary)

---

## The TensorFlow C API Problem

### The Hidden Costs of Manual Resource Management

TensorFlow provides a C API for inference that works across languages and platforms. The API is stable, documented, and portable. It is also a minefield of manual resource management.

Consider what happens when you create a tensor in the raw C API. You call `TF_AllocateTensor()`, which returns a `TF_Tensor*`. You are now responsible for calling `TF_DeleteTensor()` exactly once, after all uses complete, but before the program exits. Miss this call and you leak memory. Call it twice and you corrupt the heap. Call it while another thread is reading the tensor data and you have a data race.

The TensorFlow C API requires you to manage at least five distinct resource types, each with its own lifetime rules. `TF_Status` objects must be deleted after checking. `TF_Graph` objects must outlive all sessions using them. `TF_Session` objects must be closed before deletion. Operation descriptions created with `TF_NewOperation()` have no deletion function — abandon one without calling `TF_FinishOperation()` and the graph is corrupted.

This wrapper exists because C++ has a better answer to these problems than careful coding and code review. RAII (Resource Acquisition Is Initialization) ensures that resources are released exactly once, automatically, even when exceptions are thrown or early returns are taken. The wrapper transforms the C API's "remember to delete" contract into C++'s "impossible to forget" guarantee.

### What Goes Wrong Without RAII

The most common failure mode with raw TensorFlow C API code is the leaked status object. Every TensorFlow C function that can fail takes a `TF_Status*` output parameter. The correct pattern looks like this:

```cpp
// Pseudocode showing the C API pattern
TF_Status* status = TF_NewStatus();
TF_SessionRun(session, /*...params...*/, status);
if (TF_GetCode(status) != TF_OK) {
    const char* msg = TF_Message(status);
    TF_DeleteStatus(status);  // Must delete before throwing!
    throw std::runtime_error(msg);
}
TF_DeleteStatus(status);  // Must delete on success path too!
```

The status must be deleted on both the error path and the success path. Miss either deletion point and you leak. The wrapper's `tf::Status` class eliminates this entire category of bugs — the destructor handles deletion regardless of which path executes.

The second most common failure is the use-after-free tensor access. A function returns a pointer to tensor data, the caller stores the pointer, and later the tensor is deleted while the pointer is still in use. The wrapper's view system prevents this by tying data access to lock guard lifetime, making dangling access a compile-time impossibility rather than a runtime mystery.

---

## Architecture: How the Wrapper Works

### The Policy Template Pattern

Every major wrapper class — `Tensor`, `Graph`, `Session` — is templated on a *policy* that controls thread-safety behavior. This design, sometimes called "policy-based design," separates the what (tensor storage, graph operations) from the how (synchronization strategy).

```mermaid
graph LR
    subgraph "Policy Selection"
        P1[NoLock<br/>Zero overhead]
        P2[Mutex<br/>Exclusive access]
        P3[SharedMutex<br/>Reader-writer]
    end
    
    subgraph "Wrapper Classes"
        T[Tensor&lt;Policy&gt;]
        G[Graph&lt;Policy&gt;]
        S[Session&lt;Policy&gt;]
    end
    
    P1 --> T
    P2 --> T
    P3 --> T
    P1 --> G
    P2 --> G
    P3 --> G
    P1 --> S
    P2 --> S
    
    style P1 fill:#90EE90
    style P2 fill:#FFB6C1
    style P3 fill:#87CEEB
```

The three policies form a spectrum from speed to safety. `NoLock` provides zero synchronization overhead for single-threaded code. `Mutex` provides exclusive access for multi-threaded mutation. `SharedMutex` allows multiple concurrent readers with exclusive writers. The choice is made at compile time, ensuring zero runtime overhead for the synchronization you don't need.

This architecture means the same `Tensor` class can be instantiated as `Tensor<policy::NoLock>` for a hot loop where profiling proves no contention exists, or as `Tensor<policy::SharedMutex>` for a shared cache where reads dominate writes. The compiler generates different code for each, with no virtual dispatch and no branches to check which mode is active.

### Memory Layout and Ownership

Internally, each `Tensor` object owns a `shared_ptr` to a `TensorState` structure containing the raw `TF_Tensor*`, the shape vector, and the policy's mutex (if any). This shared ownership serves a critical safety purpose: it allows tensor views to outlive the tensor object itself.

```mermaid
graph TB
    subgraph "Tensor Object"
        T[Tensor&lt;Policy&gt;]
        SP1[shared_ptr]
    end
    
    subgraph "TensorState (shared)"
        TS[TensorState]
        TF[TF_Tensor*]
        SH[shape vector]
        MX[Policy mutex]
    end
    
    subgraph "View (temporary)"
        V[TensorView]
        SP2[shared_ptr]
        LK[lock guard]
        SPN[span&lt;T&gt;]
    end
    
    subgraph "TensorFlow Memory"
        DATA[Raw Data Buffer]
    end
    
    T --> SP1
    SP1 --> TS
    TS --> TF
    TS --> SH
    TS --> MX
    TF --> DATA
    
    V --> SP2
    SP2 --> TS
    V --> LK
    LK -.->|holds| MX
    V --> SPN
    SPN -.->|points to| DATA
    
    style TS fill:#FFE4B5
    style DATA fill:#98FB98
    style LK fill:#DDA0DD
```

When you call `tensor.read<float>()`, the returned view captures a shared reference to the tensor's state. Even if the original `Tensor` object goes out of scope while the view is still alive, the underlying data remains valid because the view's shared_ptr keeps the state alive. This design eliminates an entire class of dangling-pointer bugs that plague C API wrappers.

The `Graph` and `Session` wrappers follow a simpler ownership model. Each graph owns its `TF_Graph*` directly and deletes it in the destructor. Sessions hold a non-owning pointer to their associated graph — the graph must outlive the session. This matches TensorFlow's own lifetime requirements and is enforced by the API: you construct a session from a graph reference, not by copy.

---

## Quick Start

### Prerequisites and Integration

The wrapper requires C++20 and the TensorFlow C library. Install TensorFlow C and ensure the headers and library are in your compiler's search paths.

Integration is header-only. Include the umbrella header to access all functionality, then link against the TensorFlow C library:

```cpp
#include <tf/all.hpp>
// Link with: -ltensorflow
```

### Your First Inference

The following complete example creates a computational graph with a single constant, runs it through a session, and prints the result. This pattern — build graph, create session, run inference, extract results — applies to all TensorFlow C++ workflows.

```cpp
#include <tf/all.hpp>
#include <iostream>

int main() {
    // Create a tensor containing [1.0, 2.0, 3.0, 4.0] with shape [2, 2]
    auto input = tf::FastTensor::FromVector<float>({2, 2}, {1.0f, 2.0f, 3.0f, 4.0f});
    
    // Build a graph with a single Const operation
    tf::FastGraph graph;
    auto const_op = graph.NewOperation("Const", "my_constant")
        .SetAttrTensor("value", input.handle())
        .SetAttrType("dtype", TF_FLOAT)
        .Finish();
    
    // Create Identity to make the const fetchable
    graph.NewOperation("Identity", "output")
        .AddInput(const_op, 0)
        .Finish();
    
    // Run the graph
    tf::FastSession session(graph);
    auto results = session.Run({}, {tf::Fetch{"output", 0}});
    
    // Print results
    auto view = results[0].read<float>();
    std::cout << "Result: ";
    for (float x : view) {
        std::cout << x << " ";
    }
    std::cout << "\n";
    
    return 0;
}
```

The example uses `FastTensor`, `FastGraph`, and `FastSession` — type aliases for the `NoLock` policy variants. For single-threaded code, these provide the same functionality as the synchronized variants with no overhead.

---

## Thread Safety: Three Policies, Three Tradeoffs

### The Synchronization Spectrum

Choosing the right thread-safety policy requires understanding your access patterns. The wrong choice costs either unnecessary overhead (locks where none are needed) or correctness (no locks where races exist).

```mermaid
graph TB
    subgraph "NoLock Policy"
        N1[Thread A: read] 
        N2[Thread B: read]
        N3[Thread C: write]
        N1 -.->|no sync| N2
        N2 -.->|DATA RACE!| N3
    end
    
    subgraph "Mutex Policy"
        M1[Thread A: read<br/>🔒 exclusive]
        M2[Thread B: read<br/>⏳ waiting]
        M3[Thread C: write<br/>⏳ waiting]
        M1 -->|blocks| M2
        M2 -->|blocks| M3
    end
    
    subgraph "SharedMutex Policy"
        S1[Thread A: read<br/>🔓 shared]
        S2[Thread B: read<br/>🔓 shared]
        S3[Thread C: write<br/>⏳ waiting]
        S1 ---|concurrent| S2
        S2 -->|blocks| S3
    end
    
    style N3 fill:#FF6B6B
    style S1 fill:#90EE90
    style S2 fill:#90EE90
```

The `NoLock` policy provides zero synchronization. Its guard type is an empty struct that the compiler eliminates entirely. Use `NoLock` when tensors are created, used, and destroyed within a single thread, or when external synchronization (such as a higher-level mutex protecting an entire data structure) already prevents races.

The `Mutex` policy wraps a `std::mutex`. Every access — read or write — acquires an exclusive lock. Use `Mutex` when any thread might modify tensor data and multiple threads access the same tensor. The mutual exclusion prevents torn reads (reading partial writes) at the cost of serializing all access.

The `SharedMutex` policy wraps a `std::shared_mutex`. Read access acquires a shared lock that permits other readers. Write access acquires an exclusive lock that blocks all other access. Use `SharedMutex` when reads vastly outnumber writes, such as inference caches where models are loaded once and queried many times.

### Policy Selection in Practice

The type aliases make policy selection explicit and readable. Each core class has three aliases following a consistent naming pattern: `Fast` for `NoLock`, `Safe` for `Mutex`, `Shared` for `SharedMutex`.

```cpp
// Type aliases for Tensor
using FastTensor   = Tensor<policy::NoLock>;      // Zero overhead
using SafeTensor   = Tensor<policy::Mutex>;       // Exclusive locking
using SharedTensor = Tensor<policy::SharedMutex>; // Reader-writer locking

// Type aliases for Graph
using FastGraph   = Graph<policy::NoLock>;
using SafeGraph   = Graph<policy::Mutex>;
using SharedGraph = Graph<policy::SharedMutex>;

// Type aliases for Session
using FastSession = Session<policy::NoLock>;
using SafeSession = Session<policy::Mutex>;
```

A common pattern in inference servers is to use `FastTensor` for temporary computation buffers (created and destroyed within a single request handler) while using `SharedTensor` for shared model weights (loaded at startup, read by all request threads).

### The Lock Guard Lifetime Rule

Thread-safe tensor access works through RAII lock guards. When you call `tensor.read<float>()`, the returned view holds a lock that is released when the view is destroyed. This creates a critical rule: **the lock is held for the entire lifetime of the view**.

Extending a view's lifetime unintentionally — by storing it in a data structure, returning it from a function, or capturing it in a lambda — extends the lock's lifetime correspondingly. A view stored in a class member holds its lock until the object is destroyed. A view captured by a lambda holds its lock until the lambda is destroyed.

The callback-based access methods, `with_read()` and `with_write()`, exist specifically to make lock scope explicit. The lock is held only for the duration of the callback:

```cpp
// Lock held only during callback execution
tensor.with_read<float>([](std::span<const float> data) {
    // Lock is held here
    process(data);
});
// Lock released here, guaranteed
```

This pattern makes it impossible to accidentally extend lock lifetime. The lambda cannot escape its scope, so the lock cannot outlive the intended access window.

---

## Tensor Creation: Five Factories, Five Philosophies

### Why Five Factory Methods?

The `Tensor` class provides five static factory methods, each serving a distinct use case. Understanding when to use each prevents both bugs and unnecessary copies.

```mermaid
flowchart TD
    START[Need to create a tensor?] --> Q1{Have existing data?}
    
    Q1 -->|Yes| Q2{Data source?}
    Q1 -->|No| Q3{Need initialization?}
    
    Q2 -->|std::vector| FV[FromVector<br/>Copies data]
    Q2 -->|Single value| FS[FromScalar<br/>Rank-0 tensor]
    Q2 -->|malloc'd buffer| AM[AdoptMalloc<br/>Takes ownership]
    Q2 -->|Custom allocator| AD[Adopt<br/>Custom deallocator]
    
    Q3 -->|Yes, zeros| ZE[Zeros<br/>All zeros]
    Q3 -->|No, will overwrite| AL[Allocate<br/>Uninitialized]
    
    style FV fill:#90EE90
    style FS fill:#90EE90
    style ZE fill:#87CEEB
    style AL fill:#FFE4B5
    style AM fill:#DDA0DD
    style AD fill:#DDA0DD
```

The fundamental question each factory answers differently is: **who owns the memory, and who initializes it?**

### FromVector: Copy-In Creation

`FromVector` creates a tensor by copying data from a vector. TensorFlow allocates the tensor's internal buffer, and the factory copies the vector's contents into that buffer. The source vector can be modified or destroyed immediately after the call returns.

Use `FromVector` when you have data in a vector and want TensorFlow to own its own copy. This is the safest and most common factory for creating tensors from computed results or loaded data:

```cpp
std::vector<float> data = compute_features();
auto tensor = tf::FastTensor::FromVector<float>({batch_size, feature_dim}, data);
// data vector can now be modified or destroyed safely
```

The shape (first argument) and data size must match. A shape of `{2, 3}` requires exactly 6 elements. Mismatches throw `std::invalid_argument` with a message indicating the expected and actual counts.

### FromScalar: Single-Value Tensors

`FromScalar` creates a rank-0 tensor containing a single value. Rank-0 tensors have an empty shape (`[]`) and exactly one element. They are TensorFlow's representation of scalar values.

Use `FromScalar` for scalar constants, learning rates, or single-valued parameters:

```cpp
auto learning_rate = tf::FastTensor::FromScalar<float>(0.001f);
auto batch_size = tf::FastTensor::FromScalar<int32_t>(32);
```

### Allocate: Uninitialized Memory

`Allocate` creates a tensor with uninitialized contents. TensorFlow allocates the buffer, but no initialization occurs. The memory contains whatever values happened to be at those addresses.

Use `Allocate` when you will immediately overwrite all elements, such as output buffers for custom operations. Reading uninitialized memory is undefined behavior, so only use this factory when you can guarantee complete initialization before any read:

```cpp
auto output = tf::FastTensor::Allocate<float>({height, width, channels});
// MUST write all elements before reading any
fill_with_computed_values(output.write<float>());
```

### Zeros: Zero-Initialized Memory

`Zeros` creates a tensor with all elements set to zero (or `false` for boolean tensors). This is equivalent to `Allocate` followed by `memset`, but may be more efficient on some platforms.

Use `Zeros` for accumulators, masks initialized to false, or any case where zero is the correct initial value:

```cpp
auto accumulator = tf::FastTensor::Zeros<float>({embedding_dim});
auto visited = tf::FastTensor::Zeros<bool>({graph_size});
```

### Adopt and AdoptMalloc: External Memory Management

`Adopt` and `AdoptMalloc` create tensors that take ownership of externally allocated memory. These are advanced factories for interoperating with other allocation systems.

`AdoptMalloc` assumes the buffer was allocated with `malloc()` and will be freed with `free()`. Use this when integrating with C libraries or custom allocators that use malloc-compatible allocation:

```cpp
void* buffer = custom_allocator.allocate(num_bytes);
auto tensor = tf::FastTensor::AdoptMalloc<float>({rows, cols}, buffer, num_bytes);
// Tensor now owns buffer; will call free() on destruction
```

`Adopt` takes a custom deallocator function, enabling integration with any memory management scheme. The deallocator receives the pointer, byte count, and an optional context pointer:

```cpp
auto tensor = tf::FastTensor::Adopt(
    TF_FLOAT,
    shape,
    gpu_buffer,
    num_bytes,
    [](void* data, size_t len, void* ctx) {
        cuda_free(data);
    },
    nullptr  // deallocator context
);
```

Both `Adopt` variants validate that the provided byte length matches the expected size for the given shape and dtype. Mismatches throw `std::invalid_argument`, preventing the creation of tensors that would access out-of-bounds memory.

---

## Tensor Data Access: Views, Callbacks, and Unsafe Pointers

### The View Pattern

Tensor data access uses a view pattern that provides both safety and efficiency. A view is a combination of a `std::span` (providing range-based access to the data) and a lock guard (providing thread-safety for synchronized tensors).

The `read<T>()` method returns a read-only view. For `SharedMutex` tensors, this acquires a shared lock, allowing concurrent readers. For `Mutex` tensors, this acquires an exclusive lock. For `NoLock` tensors, this acquires an empty guard with zero overhead:

```cpp
auto view = tensor.read<float>();  // Lock acquired here
for (float x : view) {
    std::cout << x << " ";
}
// Lock released when view goes out of scope
```

The `write<T>()` method returns a writable view with exclusive access. The returned span allows modification of elements:

```cpp
auto view = tensor.write<float>();  // Exclusive lock acquired
for (float& x : view) {
    x *= 2.0f;  // Modify in place
}
// Lock released when view goes out of scope
```

### Type Safety at Access Time

The template parameter to `read<T>()` and `write<T>()` must match the tensor's actual dtype. A tensor created with `FromVector<float>` must be accessed with `read<float>()`, not `read<int>()`. Mismatches throw `std::invalid_argument` with a message indicating both the expected and actual types:

```cpp
auto tensor = tf::FastTensor::FromVector<float>({4}, {1.0f, 2.0f, 3.0f, 4.0f});
auto view = tensor.read<int>();  // Throws: "dtype mismatch: expected float32, got int32"
```

This runtime check catches type mismatches that static typing cannot. The tensor's dtype is determined at construction from runtime data, but access type is specified at compile time. The check bridges this gap.

### Callback-Based Access

The callback methods `with_read()` and `with_write()` provide an alternative access pattern that makes lock scope explicit. The callback receives a `std::span` and can return a value:

```cpp
float sum = tensor.with_read<float>([](std::span<const float> data) {
    return std::accumulate(data.begin(), data.end(), 0.0f);
});
```

This pattern prevents accidentally holding locks longer than intended. The span cannot escape the callback's scope, guaranteeing that the lock is released when the callback returns.

### Unsafe Access (Advanced)

The `unsafe_data<T>()` method returns a raw pointer with no locking. This method exists for integration with external libraries that require raw pointers and handle their own synchronization.

Using `unsafe_data()` on a synchronized tensor without external synchronization is a data race. Use this method only when you have verified that external synchronization prevents concurrent access:

```cpp
// Only safe if external_library handles synchronization
float* raw = tensor.unsafe_data<float>();
external_library_process(raw, tensor.num_elements());
```

---

## Graph Building: The Operation Builder Pattern

### The Fluent Builder Interface

Graph operations are constructed using a fluent builder pattern. The builder holds an exclusive lock on the graph for its entire lifetime, preventing concurrent modification while an operation is being configured.

```mermaid
sequenceDiagram
    participant C as Client Code
    participant G as Graph
    participant B as OperationBuilder
    participant TF as TensorFlow C API
    
    C->>G: NewOperation("MatMul", "mm")
    activate G
    G->>G: Acquire exclusive lock
    G->>TF: TF_NewOperation()
    TF-->>G: TF_OperationDescription*
    G-->>C: OperationBuilder (owns lock)
    deactivate G
    
    activate B
    C->>B: .SetAttrBool("transpose_a", false)
    B->>TF: TF_SetAttrBool()
    B-->>C: OperationBuilder&
    
    C->>B: .AddInput(other_op, 0)
    B->>TF: TF_AddInput()
    B-->>C: OperationBuilder&
    
    C->>B: .Finish()
    B->>TF: TF_FinishOperation()
    TF-->>B: TF_Operation*
    B->>B: Release lock
    B-->>C: TF_Operation*
    deactivate B
```

The `NewOperation()` method starts the builder chain. Each attribute setter returns the builder, allowing method chaining. The `Finish()` method completes the operation and releases the lock:

```cpp
auto op = graph.NewOperation("MatMul", "my_matmul")
    .AddInput(a_op, 0)
    .AddInput(b_op, 0)
    .SetAttrBool("transpose_a", false)
    .SetAttrBool("transpose_b", false)
    .Finish();  // Lock released here
```

### The Finish Requirement

Every `OperationBuilder` must have `Finish()` called exactly once. The destructor contains a debug assertion that fires if the builder is destroyed without finishing. This catches a common bug: abandoning a partially-configured operation, which corrupts the graph.

The assertion only fires in debug builds. In release builds, the destructor silently releases the lock, but the graph may be in an inconsistent state if `Finish()` was not called.

### Input and Control Dependencies

Operations connect through inputs and control dependencies. `AddInput()` connects a data output from one operation to an input of the operation being built. The second parameter is the output index, since operations can have multiple outputs:

```cpp
// Connect output 0 of prev_op to this operation
builder.AddInput(prev_op, 0);

// Connect multiple outputs from the same operation
builder.AddInput(split_op, 0);  // First split output
builder.AddInput(split_op, 1);  // Second split output
```

`AddControlInput()` creates a control dependency without data flow. The operation being built will not execute until the control input has completed, but no data is passed:

```cpp
// Ensure init_op completes before this operation runs
builder.AddControlInput(init_op);
```

### Common Attributes

TensorFlow operations use attributes to configure their behavior. The most common attributes have dedicated setter methods:

| Attribute Type | Method | Example |
|----------------|--------|---------|
| Tensor value | `SetAttrTensor(name, TF_Tensor*)` | Const operation values |
| Data type | `SetAttrType(name, TF_DataType)` | dtype specifications |
| Shape | `SetAttrShape(name, span<int64_t>)` | Output shape hints |
| Integer | `SetAttrInt(name, int64_t)` | Axis indices, sizes |
| Float | `SetAttrFloat(name, float)` | Numeric parameters |
| Boolean | `SetAttrBool(name, bool)` | Flags, modes |
| String | `SetAttrString(name, string_view)` | Padding modes, data formats |

### Operation Lookup

After building operations, you can look them up by name. `GetOperation()` returns `std::optional<tf::Operation>`, empty if the name is not found. `GetOperationOrThrow()` throws `std::runtime_error` on lookup failure:

```cpp
auto op = graph.GetOperation("my_matmul");
if (op) {
    std::cout << "Found: " << op->name() << "\n";
}

// Or, when you know it must exist:
auto op2 = graph.GetOperationOrThrow("my_matmul");
```

---

## Session Execution: Feeds, Fetches, and Targets

### The Run Method

Session execution uses `Run()` to evaluate graph operations. The method takes three arguments: feeds (input data), fetches (requested outputs), and targets (operations to run without fetching outputs).

```mermaid
flowchart LR
    subgraph Inputs
        F1[Feed: input_a]
        F2[Feed: input_b]
    end
    
    subgraph "Graph Execution"
        OP1[Placeholder A]
        OP2[Placeholder B]
        OP3[MatMul]
        OP4[Add]
        OP5[Output]
        
        F1 --> OP1
        F2 --> OP2
        OP1 --> OP3
        OP2 --> OP3
        OP3 --> OP4
        OP4 --> OP5
    end
    
    subgraph Outputs
        R1[Fetch: output]
    end
    
    OP5 --> R1
    
    style F1 fill:#98FB98
    style F2 fill:#98FB98
    style R1 fill:#87CEEB
    style OP3 fill:#FFE4B5
```

Feeds provide input tensors for placeholder operations. Each feed specifies an operation name, output index, and tensor pointer:

```cpp
tf::Feed{"input_placeholder", 0, input_tensor.handle()}
```

Fetches request output values. Each fetch specifies an operation name and output index. The returned vector contains tensors in the same order as the fetch list:

```cpp
auto results = session.Run(
    {tf::Feed{"input", 0, input.handle()}},
    {tf::Fetch{"output", 0}, tf::Fetch{"loss", 0}}
);
// results[0] is "output:0"
// results[1] is "loss:0"
```

Targets specify operations to run without returning their outputs. Use targets for operations with side effects, such as variable assignments:

```cpp
session.Run({feeds}, {}, {"assign_op", "update_op"});
```

### Feed Tensor Lifetime

**Critical:** Feed tensors must remain valid and unmodified for the duration of `Run()`. The session does not copy feed data; it reads directly from the tensor's buffer. Modifying a feed tensor while `Run()` is executing is a data race.

For synchronized tensors used as feeds, acquire a lock before calling `Run()` and hold it until `Run()` returns:

```cpp
auto guard = input_tensor.acquire_shared_lock();
auto results = session.Run(
    {tf::Feed{"input", 0, input_tensor.handle()}},
    {tf::Fetch{"output", 0}}
);
// guard released here, after Run() completes
```

### Error Handling in Run

`Run()` throws `std::runtime_error` if execution fails. Common failures include missing feed operations, type mismatches between feeds and placeholders, and graph execution errors.

When `Run()` throws, any partially-allocated output tensors are cleaned up before the exception propagates. You do not need to handle partial results.

---

## Loading SavedModels

### The Modern Model Format

SavedModel is TensorFlow's standard serialization format for production models. Unlike frozen graphs, SavedModels preserve signatures, variables, and assets in a self-contained directory structure. The wrapper provides direct loading support through `Session::LoadSavedModel()`.

### Loading a SavedModel

The `LoadSavedModel()` static factory creates both a session and its associated graph from a SavedModel directory:

```cpp
#include <tf/all.hpp>
#include <iostream>

int main() {
    // Load model - returns both session and graph
    auto [session, graph] = tf::FastSession::LoadSavedModel("/path/to/saved_model");
    
    // Create input tensor
    auto input = tf::FastTensor::FromVector<float>({1, 224, 224, 3}, image_data);
    
    // Run inference
    auto results = session.Run(
        {tf::Feed{"serving_default_input:0", input.handle()}},
        {tf::Fetch{"StatefulPartitionedCall:0"}}
    );
    
    // Process output
    auto output = results[0].read<float>();
    std::cout << "Prediction: " << output[0] << "\n";
    
    return 0;
}
```

The method returns a `std::pair<Session, Graph>` because `TF_LoadSessionFromSavedModel()` populates the graph during loading. **Both objects must be kept alive** — the session holds a non-owning pointer to the graph.

### Model Tags

SavedModels can contain multiple "metagraphs" distinguished by tags. The default tag `"serve"` identifies the inference graph. Training graphs use the tag `"train"`. Specify tags as a vector:

```cpp
// Load with default "serve" tag (most common for inference)
auto [session, graph] = tf::FastSession::LoadSavedModel("/path/to/model");

// Load with explicit tags
auto [session, graph] = tf::FastSession::LoadSavedModel(
    "/path/to/model",
    {"serve", "gpu"}  // Multiple tags for specific configurations
);
```

### Finding Operation Names

After loading a SavedModel, you may need to discover the input and output operation names. Use `Graph::GetAllOperations()` to inspect the graph:

```cpp
auto [session, graph] = tf::FastSession::LoadSavedModel("/path/to/model");

for (TF_Operation* op : graph.GetAllOperations()) {
    tf::Operation wrapped(op);
    std::cout << wrapped.name() << " : " << wrapped.op_type() << "\n";
}
```

Common patterns for SavedModel operation names:
- Inputs: `serving_default_<input_name>:0`
- Outputs: `StatefulPartitionedCall:0` or `PartitionedCall:0`

---

## Device Enumeration

### Checking Available Hardware

Before running inference, you may want to know what compute devices are available. The wrapper provides device enumeration through the session:

```cpp
tf::FastGraph graph;
// ... build or import graph ...
tf::FastSession session(graph);

// List all devices
auto devices = session.ListDevices();
std::cout << "Available devices: " << devices.count() << "\n";

for (int i = 0; i < devices.count(); ++i) {
    auto dev = devices.at(i);
    std::cout << "  " << dev.name << " (" << dev.type << ")";
    if (dev.memory_bytes > 0) {
        std::cout << " - " << (dev.memory_bytes / 1024 / 1024) << " MB";
    }
    std::cout << "\n";
}
```

### The Device Struct

Each device is represented by a `tf::Device` struct:

```cpp
struct Device {
    std::string name;         // e.g., "/device:CPU:0", "/device:GPU:0"
    std::string type;         // e.g., "CPU", "GPU"
    std::int64_t memory_bytes; // Total device memory (0 for CPU)
    
    bool is_gpu() const;      // Convenience: type == "GPU"
    bool is_cpu() const;      // Convenience: type == "CPU"
};
```

### Quick GPU Check

For the common case of "do I have a GPU?", use the convenience method:

```cpp
if (session.HasGPU()) {
    std::cout << "GPU acceleration available\n";
} else {
    std::cout << "Running on CPU only\n";
}
```

### Device Placement

Device enumeration tells you what's available; TensorFlow handles automatic placement. For explicit device placement (forcing specific operations to specific devices), use the `OperationBuilder::SetDevice()` method when building graphs:

```cpp
graph.NewOperation("MatMul", "gpu_matmul")
    .AddInput(a, 0)
    .AddInput(b, 0)
    .SetDevice("/device:GPU:0")  // Force GPU execution
    .Finish();
```

Note: Explicit placement requires the device to exist. Query `ListDevices()` first to verify availability.

---

## Error Handling Model

### Exception-Based Error Reporting

The wrapper uses exceptions for error reporting. This matches C++ conventions and enables natural integration with existing exception-handling code.

All wrapper methods that can fail throw exceptions rather than returning error codes. The thrown type is always `std::runtime_error` or a derived standard exception type. The message contains context including the TensorFlow error code name, file location, and the underlying TensorFlow error message.

### The Status Class

The `tf::Status` class wraps TensorFlow's `TF_Status` type, providing RAII lifetime management and convenient error checking. Most users interact with `Status` indirectly through the wrapper's exception-throwing methods, but direct use is sometimes needed when calling raw TensorFlow C functions.

The `throw_if_error()` method checks the status and throws if it indicates failure. The thrown message includes source location information captured automatically through `std::source_location`:

```cpp
tf::Status status;
TF_SomeFunction(/*...params...*/, status.get());
status.throw_if_error("TF_SomeFunction");
// If status was not OK, throws with message including "TF_SomeFunction",
// the error code name, and the TensorFlow error message
```

### Error Code Names

TensorFlow error codes have human-readable names accessible through `Status::code_to_string()`. The wrapper includes these names in exception messages, making debugging easier than raw numeric codes.

The error codes follow Google's canonical error space, familiar from gRPC and other Google APIs. Common codes include `INVALID_ARGUMENT` (bad input), `NOT_FOUND` (missing resource), `ALREADY_EXISTS` (duplicate creation), and `INTERNAL` (unexpected TensorFlow failure).

---

## Performance Rules of Thumb

### Choose the Minimal Policy

Use `FastTensor` (NoLock policy) for all tensors unless you have proven concurrent access exists. The policy choice has no effect on correctness in single-threaded code, but affects performance. NoLock has zero synchronization overhead; Mutex and SharedMutex have lock acquisition costs.

### Prefer Views Over Copies

View-based access (`read<T>()`, `write<T>()`) provides direct access to tensor memory without copying. Copying view contents into vectors, then processing the vectors, doubles memory traffic. Process data in place when possible:

```cpp
// Efficient: process in place
tensor.with_write<float>([](std::span<float> data) {
    for (float& x : data) x = std::sqrt(x);
});

// Inefficient: unnecessary copy
auto view = tensor.read<float>();
std::vector<float> copy(view.begin(), view.end());
for (float& x : copy) x = std::sqrt(x);
// Then what? Copy discarded, original unchanged
```

### Batch Small Tensors

Creating and destroying many small tensors has allocation overhead. When processing many small inputs, batch them into larger tensors when possible. TensorFlow's batch dimension (typically axis 0) exists for this purpose.

### Pre-allocate Output Buffers

For repeated inference with fixed output shapes, consider pre-allocating output tensors and reusing them across calls. This avoids allocation in the hot path, though it requires managing the buffers yourself rather than letting `Run()` allocate fresh outputs.

---

## When to Use This Library (and When Not To)

### Use This Library When

You need TensorFlow inference in a C++ application and want memory safety without the complexity of building TensorFlow's C++ API from source. The wrapper provides a clean, modern C++ interface to TensorFlow's stable C API.

You have multi-threaded inference requirements and want compile-time policy selection for thread safety. The policy template pattern lets you choose synchronization strategy without runtime overhead for the policies you don't use.

You value RAII guarantees and want automatic resource cleanup. The wrapper ensures that TensorFlow resources are always released, even in the presence of exceptions.

### Do Not Use This Library When

You need training, not just inference. TensorFlow's Python API is the primary training interface. This library wraps the C API, which provides inference capabilities but limited training support.

You need maximum performance and can accept the complexity of raw C API usage. The wrapper adds some overhead (virtual dispatch in view destructors, exception checking) that raw C code avoids. For most applications this overhead is negligible compared to TensorFlow operation costs, but in extreme cases every microsecond matters.

You need features beyond the C API's capabilities, such as custom gradient functions or distributed training coordination.

---

## Migration from Raw TensorFlow C API

### Alternatives

- **TensorFlow C++ API** — Official C++ bindings, requires building TensorFlow from source
- **TensorFlow Lite C++ API** — Mobile-focused, different model format
- **ONNX Runtime** — Multi-framework inference, different API style
- **PyTorch C++ (LibTorch)** — Alternative framework, requires PyTorch models

### API Mapping

| TensorFlow C API | This Wrapper | Notes |
|------------------|--------------|-------|
| `TF_NewStatus()` / `TF_DeleteStatus()` | `tf::Status` | Automatic cleanup |
| `TF_AllocateTensor()` / `TF_DeleteTensor()` | `tf::Tensor::Allocate<T>()` | Type-safe, RAII |
| `TF_TensorData()` | `tensor.read<T>()`, `tensor.write<T>()` | Thread-safe views |
| `TF_NewGraph()` / `TF_DeleteGraph()` | `tf::Graph<Policy>` | Automatic cleanup |
| `TF_NewOperation()` / `TF_FinishOperation()` | `graph.NewOperation().Finish()` | Fluent builder |
| `TF_NewSession()` / `TF_DeleteSession()` | `tf::Session<Policy>` | Automatic cleanup |
| `TF_SessionRun()` | `session.Run()` | Exception on error |
| `TF_LoadSessionFromSavedModel()` | `Session::LoadSavedModel()` | Returns session + graph |
| `TF_SessionListDevices()` | `session.ListDevices()` | RAII DeviceList |

### Migration Steps

First, replace status handling. Change `TF_Status*` variables to `tf::Status` objects, and replace manual `TF_GetCode()` checks with `status.throw_if_error()`:

```cpp
// Before:
TF_Status* status = TF_NewStatus();
TF_SomeFunction(/*...*/, status);
if (TF_GetCode(status) != TF_OK) {
    std::string msg = TF_Message(status);
    TF_DeleteStatus(status);
    throw std::runtime_error(msg);
}
TF_DeleteStatus(status);

// After:
tf::Status status;
TF_SomeFunction(/*...*/, status.get());
status.throw_if_error("TF_SomeFunction");
```

Second, replace tensor allocation. Change `TF_AllocateTensor()` calls to the appropriate factory method:

```cpp
// Before:
int64_t dims[] = {batch, height, width, channels};
TF_Tensor* t = TF_AllocateTensor(TF_FLOAT, dims, 4, batch * height * width * channels * sizeof(float));
// ... use tensor ...
TF_DeleteTensor(t);

// After:
auto tensor = tf::FastTensor::Allocate<float>({batch, height, width, channels});
// No manual deletion needed
```

Third, replace data access. Change `TF_TensorData()` casts to typed views:

```cpp
// Before:
float* data = static_cast<float*>(TF_TensorData(tensor));
for (int i = 0; i < num_elements; ++i) {
    data[i] = compute(i);
}

// After:
auto view = tensor.write<float>();
for (size_t i = 0; i < view.size(); ++i) {
    view[i] = compute(i);
}
```

---

## Troubleshooting

### Compilation Errors

**"concept 'TensorScalar' not satisfied"**

The type you passed to a tensor factory or view method is not a supported scalar type. The wrapper supports `float`, `double`, all standard integer types (`int8_t` through `uint64_t`), and `bool`. Complex types, strings, and custom types are not supported.

**"no matching function for call to 'FromVector'"**

The shape and data types may not match. `FromVector<float>` requires a `std::vector<float>`, not `std::vector<double>`. Also check that shape is `std::span<const int64_t>` or convertible (vector, initializer_list).

**"static assertion failed: OperationBuilder destroyed without calling Finish()"**

In debug builds, the operation builder asserts that `Finish()` was called. Check that every `NewOperation()` chain ends with `.Finish()`. If you're storing the builder temporarily, ensure all code paths call `Finish()`.

### Runtime Errors

**"dtype mismatch: expected float32, got int32"**

You called `read<int>()` or `write<int>()` on a tensor created with a different type. Use the same type parameter as the tensor's creation factory.

**"shape requires N elements, got M"**

The `FromVector` call received a shape and data with mismatched element counts. A shape of `{2, 3}` requires exactly 6 elements.

**"Feed operation 'X' not found" or "Fetch operation 'X' not found"**

The operation name passed to `Run()` does not exist in the graph. Check spelling and verify the operation was added before creating the session.

### Performance Issues

**Lock contention with SafeTensor/SharedTensor**

If profiling shows time spent waiting for tensor locks, consider whether the tensors need synchronization. Tensors used only within a single thread should use `FastTensor`. Tensors that are read-heavy and write-rare should use `SharedTensor` instead of `SafeTensor`.

**Excessive allocation in inference loops**

If creating new tensors on every inference call, consider pre-allocating and reusing buffers. Use `Allocate` or `Zeros` to create reusable output tensors outside the loop.

---

## API Reference

### Type Aliases

| Alias | Expansion | Use Case |
|-------|-----------|----------|
| `FastTensor` | `Tensor<policy::NoLock>` | Single-threaded, zero overhead |
| `SafeTensor` | `Tensor<policy::Mutex>` | Multi-threaded, exclusive access |
| `SharedTensor` | `Tensor<policy::SharedMutex>` | Multi-threaded, reader-writer |
| `FastGraph` | `Graph<policy::NoLock>` | Single-threaded graph building |
| `SafeGraph` | `Graph<policy::Mutex>` | Multi-threaded graph building |
| `SharedGraph` | `Graph<policy::SharedMutex>` | Multi-threaded with concurrent reads |
| `FastSession` | `Session<policy::NoLock>` | Single-threaded inference |
| `SafeSession` | `Session<policy::Mutex>` | Multi-threaded inference |

### Tensor Factories

| Method | Creates | Initializes | Owns Memory |
|--------|---------|-------------|-------------|
| `FromVector<T>(shape, vec)` | Tensor from vector | Copies vec contents | TensorFlow |
| `FromScalar<T>(value)` | Rank-0 tensor | Single value | TensorFlow |
| `Allocate<T>(shape)` | Uninitialized tensor | No | TensorFlow |
| `Zeros<T>(shape)` | Zero-filled tensor | All zeros | TensorFlow |
| `AdoptMalloc<T>(shape, ptr, bytes)` | From malloc'd buffer | Preserves contents | Caller then TF (free) |
| `Adopt(dtype, shape, ptr, bytes, dealloc)` | From custom buffer | Preserves contents | Custom deallocator |

### Tensor Access Methods

| Method | Lock Type | Returns | Use Case |
|--------|-----------|---------|----------|
| `read<T>()` | Shared | `ReadView<T>` | Thread-safe reading |
| `write<T>()` | Exclusive | `WriteView<T>` | Thread-safe writing |
| `with_read<T>(fn)` | Shared | fn's return type | Scoped reading |
| `with_write<T>(fn)` | Exclusive | fn's return type | Scoped writing |
| `unsafe_data<T>()` | None | `T*` | External sync only |

### Tensor Queries

| Method | Returns | Notes |
|--------|---------|-------|
| `shape()` | `const vector<int64_t>&` | Dimension sizes |
| `rank()` | `int` | Number of dimensions |
| `dtype()` | `TF_DataType` | TensorFlow type enum |
| `dtype_name()` | `const char*` | Human-readable type |
| `byte_size()` | `size_t` | Total bytes |
| `num_elements()` | `size_t` | Product of dimensions |
| `empty()` | `bool` | True if null handle |
| `handle()` | `TF_Tensor*` | Raw handle |

### Graph Methods

| Method | Returns | Notes |
|--------|---------|-------|
| `NewOperation(type, name)` | `OperationBuilder` | Starts builder, acquires lock |
| `GetOperation(name)` | `optional<Operation>` | Lookup by name |
| `GetOperationOrThrow(name)` | `Operation` | Throws if not found |
| `Import(graphdef, prefix)` | void | Import from serialized graph |
| `handle()` | `TF_Graph*` | Raw handle |

### Session Methods

| Method | Returns | Notes |
|--------|---------|-------|
| `Run(feeds, fetches)` | `vector<Tensor<>>` | Basic inference |
| `Run(feeds, fetches, targets)` | `vector<Tensor<>>` | With target ops |
| `ListDevices()` | `DeviceList` | Enumerate compute devices |
| `HasGPU()` | `bool` | Quick GPU availability check |
| `LoadSavedModel(path, tags, opts)` | `pair<Session, Graph>` | Static factory for SavedModel |
| `handle()` | `TF_Session*` | Raw handle |

### Device and DeviceList

| Type/Method | Returns | Notes |
|-------------|---------|-------|
| `Device::name` | `std::string` | e.g., "/device:GPU:0" |
| `Device::type` | `std::string` | e.g., "GPU", "CPU" |
| `Device::memory_bytes` | `int64_t` | Device memory (0 for CPU) |
| `Device::is_gpu()` | `bool` | Convenience check |
| `Device::is_cpu()` | `bool` | Convenience check |
| `DeviceList::count()` | `int` | Number of devices |
| `DeviceList::at(index)` | `Device` | Get device by index |
| `DeviceList::all()` | `vector<Device>` | Get all devices |

---

## FAQ

**Q: Can I use this with TensorFlow 1.x models?**

A: Yes, the C API is stable across TensorFlow versions. Models saved in TensorFlow 1.x GraphDef format work with TensorFlow 2.x C libraries.

**Q: How do I load a SavedModel?**

A: Use `Session::LoadSavedModel()`:

```cpp
auto [session, graph] = tf::FastSession::LoadSavedModel("/path/to/model");
auto results = session.Run({tf::Feed{"input:0", tensor}}, {tf::Fetch{"output:0"}});
```

The method returns both a session and the populated graph. Keep both alive for the session's lifetime.

**Q: Is there GPU support?**

A: Yes. GPU execution works automatically if TensorFlow was built with GPU support. Use `session.HasGPU()` to check availability, and `session.ListDevices()` to enumerate all devices:

```cpp
if (session.HasGPU()) {
    std::cout << "GPU available\n";
}
for (auto& dev : session.ListDevices().all()) {
    std::cout << dev.name << " (" << dev.type << ")\n";
}
```

TensorFlow handles device placement automatically. For explicit placement, use `OperationBuilder::SetDevice()` when building graphs.

**Q: Can I mix policies in one application?**

A: Yes. Different tensors, graphs, and sessions can use different policies. A common pattern uses `FastTensor` for temporary computation buffers and `SharedTensor` for shared caches.

**Q: What happens if I forget to call Finish() on an OperationBuilder?**

A: In debug builds, the destructor fires an assertion. In release builds, the lock is released but the operation may not be properly registered in the graph. Always ensure `Finish()` is called.

---

## Glossary

**Device**: A compute unit available for TensorFlow operations. Common types are CPU and GPU. Enumerated via `session.ListDevices()`.

**DeviceList**: RAII wrapper for `TF_DeviceList`. Provides access to available compute devices through `count()`, `at()`, and `all()` methods.

**dtype**: TensorFlow's data type enumeration (`TF_FLOAT`, `TF_INT32`, etc.). Determines element size and interpretation.

**Feed**: An input tensor provided to `Session::Run()`. Maps an operation output to externally-supplied data.

**Fetch**: A requested output from `Session::Run()`. The session computes the operation and returns the result.

**Guard**: A RAII lock holder. The lock is acquired on construction and released on destruction.

**NoLock Policy**: A thread-safety policy with zero synchronization overhead. The guard type is empty and optimized away.

**Operation**: A node in a TensorFlow computational graph. Has a type (like "MatMul"), a name (unique within the graph), inputs, outputs, and attributes.

**Policy**: A template parameter controlling thread-safety behavior. The three policies are `NoLock`, `Mutex`, and `SharedMutex`.

**Rank**: The number of dimensions in a tensor's shape. A scalar has rank 0, a vector has rank 1, a matrix has rank 2.

**SavedModel**: TensorFlow's standard model serialization format. A directory containing `saved_model.pb` and associated assets. Loaded via `Session::LoadSavedModel()`.

**Shape**: The dimensions of a tensor. A shape of `{2, 3, 4}` describes a 3D tensor with 24 total elements.

**Tag**: A string identifier for a metagraph within a SavedModel. Common tags: `"serve"` (inference), `"train"` (training).

**Target**: An operation to run in `Session::Run()` without fetching its output. Used for side-effecting operations.

**View**: A non-owning reference to tensor data combined with a lock guard. Provides safe, temporary access to memory owned by the tensor.

---

*TensorFlow C++20 Wrapper — User Manual v1.1*

*Last verified: 2025-01-21*
