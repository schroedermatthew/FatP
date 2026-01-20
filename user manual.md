
# TensorFlow‑C++20 Wrapper – User Manual  

*Version:* 1.0.0 (C++20)  
*Author:* Your Name  
*License:* MIT  

---  

## Table of Contents  

1. [Overview](#overview)  
2. [Prerequisites](#prerequisites)  
3. [Directory Layout & Installation](#directory-layout--installation)  
4. [Core Concepts](#core-concepts)  
    - 4.1 [Lock‑Policy Model](#41-lock‑policy-model)  
    - 4.2 [Error Handling](#42-error-handling)  
5. [Headers Overview](#headers-overview)  
    - 5.1 [`tf/policy.hpp`](#51-tfpolicyhpp)  
    - 5.2 [`tf/status.hpp`](#52-tfstatushpp)  
    - 5.3 [`tf/tensor.hpp`](#53-tftensorhpp)  
    - 5.4 [`tf/graph.hpp`](#54-tfgraphhpp)  
    - 5.5 [`tf/operation.hpp`](#55-tfoperationhpp)  
    - 5.6 [`tf/session.hpp`](#56-tfsessionhpp)  
    - 5.7 [`tf/device_list.hpp`](#57-tfdevice_listhpp)  
    - 5.8 [`tf/buffer_builder.hpp`](#58-tfbuffer_builderhpp)  
    - 5.9 [`tf/saved_model.hpp`](#59-tfsaved_modelhpp)  
    - 5.10 [`tf/all.hpp`](#5a-tfallhpp)  
6. [Getting Started – A Minimal Example](#getting-started---a-minimal-example)  
7. [Detailed API Usage](#detailed-api-usage)  
    - 7.1 [Creating & Manipulating Tensors](#71-creating--manipulating-tensors)  
    - 7.2 [Building a Graph](#72-building-a-graph)  
    - 7.3 [Running a Session](#73-running-a-session)  
    - 7.4 [Thread‑Safe Interaction](#74-thread‑safe-interaction)  
    - 7.5 [Device Enumeration](#75-device-enumeration)  
    - 7.6 [Using `tf::BufferBuilder`](#76-using-tfbufferbuilder)  
    - 7.7 [Loading a SavedModel](#77-loading-a-savedmodel)  
8. [Best Practices & Performance Tips](#best-practices--performance-tips)  
9. [Troubleshooting & FAQ](#troubleshooting--faq)  
10. [Extending the Wrapper](#extending-the-wrapper)  
11. [Building & Running the Test Suite](#building--running-the-test-suite)  

---  

## Overview  

This library provides a **modern, header‑only C++ façade** for the low‑level TensorFlow C API (`tensorflow/c/c_api.h`).  
Key design goals are:

* **RAII** – every TensorFlow opaque handle (`TF_Tensor`, `TF_Graph`, `TF_Session`, …) is wrapped in a class that correctly frees the resource.  
* **Zero‑overhead thread safety** – a *policy* template argument (`NoLock`, `Mutex`, `SharedMutex`) injects synchronization only when you ask for it. The default (`NoLock`) incurs **no runtime cost**.  
* **C++20 ergonomics** – `std::span`, concepts, `std::format`, and `std::source_location` give compile‑time safety and expressive diagnostics.  
* **Exception‑based error handling** – TensorFlow status codes are automatically translated into `std::runtime_error` (or derived) exceptions with rich context.  

The library is deliberately **thin** – it simply mirrors the TensorFlow C API with safer C++ wrappers, leaving advanced features (e.g., custom ops, profiling) open for extension.

---  

## Prerequisites  

| Item | Minimum Version | Why |
|------|----------------|-----|
| **C++ compiler** | **C++20** (GCC 10+, Clang 12+, MSVC 2019 16.10+, AppleClang 12+) | Uses concepts, `std::span`, `std::format`, `std::source_location`. |
| **TensorFlow C library** | `libtensorflow` 2.10+ (any version that provides the C API) | The wrapper only forwards calls; you must have the shared library and headers. |
| **CMake** (optional, but recommended) | 3.14+ | Simplifies building examples and tests. |
| **Boost.Test** (for the supplied test suite) | 1.70+ | Header‑only, no linking required for the core library. |
| **Google Benchmark** (optional for performance testing) | – | Not required for normal use. |

### Installing the TensorFlow C library  

*Linux (Ubuntu example)*  

```bash
# Download the prebuilt libtensorflow (adjust version as needed)
wget https://storage.googleapis.com/tensorflow/libtensorflow/libtensorflow-cpu-linux-x86_64-2.10.0.tar.gz
sudo tar -C /usr/local -xzf libtensorflow-cpu-linux-x86_64-2.10.0.tar.gz
sudo ldconfig
```

*Windows* – download the ZIP from the same URL and add the `dll` directory to `%PATH`.  

*macOS* – use the `libtensorflow-cpu-darwin` archive and place the `.dylib` in `/usr/local/lib`.

After installing, the headers should be available under `<tensorflow/c/c_api.h>` and the shared library should be discoverable by the linker (`-ltensorflow` on Linux).

---  

## Directory Layout & Installation  

```
project_root/
│   CMakeLists.txt          ← optional (example build file)
│   main.cpp                ← usage example
│
└── include/
    └── tf/
        tf/policy.hpp
        tf/status.hpp
        tf/tensor.hpp
        tf/graph.hpp
        tf/operation.hpp
        tf/session.hpp
        tf/device_list.hpp
        tf/buffer_builder.hpp
        tf/saved_model.hpp   ← optional
        tf/all.hpp
```

*Copy the `include/tf/` directory into your project or install it into a system include path.*  
All headers are **self‑contained**, i.e. they only include the TensorFlow C header (`tensorflow/c/c_api.h`) and the C++ standard library.

### CMake integration (optional)

A minimal `CMakeLists.txt` to build the demo:

```cmake
cmake_minimum_required(VERSION 3.14)
project(tf_cpp20_demo LANGUAGES CXX)

#--- Find TensorFlow C library ------------------------------------------------
set(TF_ROOT "/usr/local")   # Change if TF is installed elsewhere
find_path(TF_INC_DIR tensorflow/c/c_api.h
          PATHS "${TF_ROOT}/include")
find_library(TF_LIB tensorflow
             PATHS "${TF_ROOT}/lib"
                   "${TF_ROOT}/lib64")

if(NOT TF_INC_DIR OR NOT TF_LIB)
    message(FATAL_ERROR "TensorFlow C headers or library not found")
endif()

#--- Build ---------------------------------------------------------------
add_executable(tf_demo main.cpp)

target_include_directories(tf_demo PRIVATE
    ${TF_INC_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(tf_demo PRIVATE ${TF_LIB})
target_compile_features(tf_demo PRIVATE cxx_std_20)
```

Build with:

```bash
mkdir -p build && cd build
cmake .. && cmake --build .
./tf_demo
```

---  

## Core Concepts  

### 4.1 Lock‑Policy Model  

All major wrapper classes (`Tensor`, `Graph`, `Session`) accept a **policy template argument** that implements the `tf::policy::LockPolicy` concept:

| Policy | Semantics | When to use |
|--------|-----------|-------------|
| `policy::NoLock` | No synchronization. All methods are effectively *no‑ops* for locking. | Single‑threaded programs, or when the user manually synchronises external access. |
| `policy::Mutex` | Exclusive `std::mutex`. Every public method obtains an exclusive lock. | Multiple threads may **write** or **run** on the same object simultaneously. |
| `policy::SharedMutex` | Readers acquire a shared lock (`std::shared_mutex`), writers acquire exclusive. | Frequent read‑only operations (e.g., `Graph::GetOperation`) from many threads while occasional mutations occur. |

The policy is a *template argument*, so **the compiler can completely eliminate the lock code** when `NoLock` is used.  

```cpp
// No lock – fastest, no runtime overhead
tf::Tensor<> t = tf::Tensor<>::FromScalar<float>(3.14f);

// Thread‑safe session – each Run() serialises internally
tf::Session<tf::policy::Mutex> sess(my_graph);
```

### 4.2 Error Handling  

All TensorFlow status objects are wrapped by the helper `tf::ThrowIfNotOK`.  
It throws a `std::runtime_error` (or a derived class) that includes:

* The user‑provided prefix (e.g. `"TF_NewSession"`).  
* The source file and line (`std::source_location`).  
* The message returned by TensorFlow.

```cpp
TF_Status* st = TF_NewStatus();
TF_GraphImportGraphDef(g.handle(), buf, nullptr, st);
tf::ThrowIfNotOK(st, "ImportGraphDef");   // throws on failure
```

Because the wrapper throws on any non‑OK status, **you never have to check return codes** manually – just use `try / catch` blocks.

---  

## Headers Overview  

Below is a brief description of each public header.  For the full API, consult the comments inside the headers themselves.

### 5.1 `tf/policy.hpp`

*Definitions*:  

```cpp
struct NoLock;
class Mutex;
class SharedMutex;
template<class P> concept LockPolicy;
```

*Provides*:  

* Empty `NoLock` for zero‑overhead.  
* `Mutex` (exclusive) and `SharedMutex` (read‑write).  
* Concept that guarantees the required interface (`lock()`, `unlock()`, `scoped_lock()`).  

### 5.2 `tf/status.hpp`

*Helper* `ThrowIfNotOK(TF_Status*, std::string_view prefix = "", std::source_location = ...)`.  
Uses `std::format` to produce a helpful error message and then throws `std::runtime_error`.

### 5.3 `tf/tensor.hpp`

*Template* `template<class Policy = tf::policy::NoLock> class Tensor`.  

Important members:

| Member | Description |
|--------|-------------|
| `static Tensor FromVector(std::span<const std::int64_t> dims, const std::vector<T>& values)` | Copies data into a newly allocated tensor. |
| `static Tensor FromScalar(T value)` | Creates a rank‑1 tensor containing a single scalar. |
| `static Tensor FromRaw(TF_Tensor* raw)` | **Adopts** ownership of an existing TF_Tensor (used by Session). |
| `T* data()` / `const T* data() const` | Typed access; checks the dtype at runtime. |
| `TF_Tensor* handle() const` | Raw handle for low‑level API calls. |
| Move constructor / move assignment – **no copies**. |
| `TensorScalar` concept restricts allowed scalar types (float, double, int32, int64, uint8). |

### 5.4 `tf/graph.hpp`

*Template* `template<class Policy = tf::policy::NoLock> class Graph`.  

Key functionality:

| Member | Description |
|--------|-------------|
| `void ImportGraphDef(const void* proto, std::size_t len, const TF_ImportGraphDefOptions* = nullptr)` | Imports a serialized GraphDef protobuf. |
| `std::optional<TF_Operation*> GetOperation(const std::string& name) const` | Returns a pointer wrapped in `std::optional`; forces callers to handle the “not‑found” case. |
| `OperationBuilder NewOperation(const std::string& type, const std::string& name)` | RAII builder for creating a new op (set attributes, add inputs, `Finish()`). |
| `TF_Graph* handle() const` | Needed by `Session`. |

### 5.5 `tf/operation.hpp`

*Thin, non‑owning wrapper* around `TF_Operation*`.  

Provides convenience queries:

```cpp
std::string name() const;
std::string type() const;
std::string device() const;
int num_inputs() const;
int num_outputs() const;
TF_Output output(int idx = 0) const;
```

### 5.6 `tf/session.hpp`

*Template* `template<class Policy = tf::policy::NoLock> class Session`.  

Main API:

| Member | Description |
|--------|-------------|
| `Session(Graph<> &graph, const SessionOptions* = nullptr, const void* config = nullptr, std::size_t = 0)` | Constructs a TF_Session bound to a graph. |
| `std::vector<Tensor<>> Run(const std::vector<Feed>& feeds, const std::vector<Fetch>& fetches, const std::vector<std::string>& targets = {}) const` | Executes the graph.  `Feed` contains a name, output index, and a `Tensor<>`. `Fetch` contains name + index. |
| `TF_Session* handle() const` | Low‑level handle for advanced usage. |
| All public methods are guarded by `policy_.scoped_lock()`.  

### 5.7 `tf/device_list.hpp`

*Read‑only view* of devices visible to a session (`TF_DeviceList`).  

```cpp
int size() const;
std::string name(int i) const;
std::string type(int i) const;
std::size_t memory_limit(int i) const;
std::uint64_t incarnation(int i) const;
```

### 5.8 `tf/buffer_builder.hpp`

Utility to **incrementally build a `TF_Buffer`** (useful for constructing protobuf blobs manually).  

```cpp
BufferBuilder& Append(const void* data, std::size_t len);
BufferBuilder& AppendString(const std::string&);
BufferBuilder& AppendScalar<T>(const T&);
Buffer Buffer Build();   // returns a tf::Buffer that owns the memory.
```

### 5.9 `tf/saved_model.hpp` *(optional)*  

High‑level loader for TensorFlow SavedModel directories.  

```cpp
SavedModel(const std::string& export_dir,
           const std::vector<std::string>& tags,
           const SessionOptions* = nullptr,
           const void* run_options = nullptr,
           std::size_t run_options_len = 0);
tf::Session& session();     // access the underlying Session
tf::Graph&   graph();       // underlying graph
```

### 5.10 `tf/all.hpp`

Convenient umbrella header that includes *all* of the above.

---  

## Getting Started – A Minimal Example  

```cpp
// main.cpp
#include "tf/all.hpp"
#include <iostream>

using namespace tf;

int main()
{
    // ---------------------------------------------------------
    // 1️⃣  Build a tiny graph: Const → Identity
    // ---------------------------------------------------------
    Graph<> g;                                 // default NoLock (fast)
    std::vector<std::int64_t> shape = {1, 4};
    std::vector<float> values = {0.1f, 0.2f, 0.3f, 0.4f};

    Tensor<> const_tensor = Tensor<>::FromVector<float>(shape, values);

    // Const node
    auto const_builder = g.NewOperation("Const", "c0");
    const_builder.SetAttrTensor("value", const_tensor.handle())
                 .SetAttrType("dtype", TF_FLOAT);
    TF_Operation* c_op = const_builder.Finish();

    // Identity node (takes c0 as input)
    auto id_builder = g.NewOperation("Identity", "out");
    id_builder.AddInput({c_op, 0});
    id_builder.Finish();

    // ---------------------------------------------------------
    // 2️⃣  Run the graph (single‑threaded, NoLock session)
    // ---------------------------------------------------------
    Session<> sess(g);
    std::vector<Fetch> fetches = { {"out", 0} };
    auto results = sess.Run({}, fetches);
    const float* out = results[0].data<float>();

    std::cout << "Result: ";
    for (std::size_t i = 0; i < values.size(); ++i)
        std::cout << out[i] << ' ';
    std::cout << '\n';
}
```

Compile (Linux example):

```bash
g++ -std=c++20 -I./include -I/path/to/tensorflow/include \
    main.cpp -o tf_demo -L/path/to/tensorflow/lib -ltensorflow -pthread
./tf_demo
```

You should see:

```
Result: 0.1 0.2 0.3 0.4 
```

---  

## Detailed API Usage  

### 7.1 Creating & Manipulating Tensors  

```cpp
// 1️⃣  From a vector (copies data)
std::vector<std::int64_t> shape = {2, 3};
std::vector<float> values = {1,2,3,4,5,6};
Tensor<> t = Tensor<>::FromVector<float>(shape, values);

// 2️⃣  From a scalar (rank‑1)
Tensor<> scalar = Tensor<>::FromScalar<int64_t>(42);

// 3️⃣  Access data (read‑write)
float* data = t.data<float>();           // exclusive lock (if policy enables it)
for (int i = 0; i < 6; ++i) data[i] *= 2.0f;

// 4️⃣  Access data (read‑only)
const float* ro = t.data<float>();      // shared lock if policy permits

// 5️⃣  Shape query
for (auto d : t.shape()) std::cout << d << ' ';   // prints "2 3"
```

#### Custom Deallocator  

If you have memory managed elsewhere (e.g., a memory pool), provide your own deallocator when constructing the tensor:

```cpp
void my_deallocator(void* data, std::size_t, void*) {
    my_pool.free(data);
}
void* my_buf = my_pool.alloc(1024);
Tensor<> custom(TF_UINT8,
                std::span<const std::int64_t>{1},
                my_buf,
                1024,
                my_deallocator,
                nullptr);
```

### 7.2 Building a Graph  

```cpp
Graph<> g;

// 1️⃣  Constant operation
Tensor<> const_tensor = Tensor<>::FromScalar<float>(3.14f);
auto const_builder = g.NewOperation("Const", "my_const");
const_builder.SetAttrTensor("value", const_tensor.handle())
             .SetAttrType("dtype", TF_FLOAT);
TF_Operation* const_op = const_builder.Finish();

// 2️⃣  Another op, e.g., Relu (requires a tensor input)
auto relu_builder = g.NewOperation("Relu", "relu");
relu_builder.AddInput({const_op, 0});
relu_builder.Finish();

// 3️⃣  Lookup an operation (read‑only)
if (auto op_opt = g.GetOperation("relu")) {
    Operation op(*op_opt);
    std::cout << "Found op: " << op.name()
              << " of type " << op.type() << '\n';
}
```

**Note:** The graph is *mutable* only while you are building it. After construction you can safely share the graph across threads **if you use a `SharedMutex` policy** (`Graph<policy::SharedMutex>`).  

### 7.3 Running a Session  

```cpp
// Session with default NoLock (single thread)
Session<> sess(g);

// No feeds are needed because the graph already contains a Const.
// Fetch the output of the Relu node.
std::vector<Fetch> fetches = { {"relu", 0} };
auto out_tensors = sess.Run({}, fetches);
const float* out = out_tensors[0].data<float>();
std::cout << "Relu output: " << out[0] << '\n';
```

**Running with a mutex (thread‑safe):**

```cpp
Session<policy::Mutex> safe_sess(g);
std::vector<std::thread> workers;
for (int i = 0; i < 4; ++i) {
    workers.emplace_back([&]{
        auto result = safe_sess.Run({}, fetches);
        // process result …
    });
}
for (auto& t : workers) t.join();
```

Because the `Mutex` policy protects the whole `Run` call, you never need additional synchronisation.

### 7.4 Thread‑Safe Interaction  

| Situation | Recommended Policy | Example |
|-----------|-------------------|---------|
| **Only one thread ever accesses the object** | `policy::NoLock` | `Graph<> g; Session<> s(g);` |
| **Many threads call `Session::Run` concurrently** | `policy::Mutex` | `Session<policy::Mutex> s(g);` |
| **Many threads query `Graph::GetOperation` while a rare thread mutates the graph** | `policy::SharedMutex` | `Graph<policy::SharedMutex> g;` |
| **Explicit external synchronisation (e.g., you already have a mutex)** | `policy::NoLock` and lock externally | `std::mutex external; { std::lock_guard lock(external); sess.Run(...); }` |

**Performance Note:**  
When using `Mutex` the critical section includes the whole `TF_SessionRun`, which may take milliseconds. The lock cost is negligible compared to the compute time.  

### 7.5 Device Enumeration  

```cpp
Session<> sess(g);
DeviceList devs(sess.handle());
std::cout << "Available devices (" << devs.size() << "):\n";
for (int i = 0; i < devs.size(); ++i) {
    std::cout << "  " << i << ": " << devs.name(i)
              << " [" << devs.type(i) << "]\n";
}
```

Typical output (CPU‑only build):

```
Available devices (1):
  0: /job:localhost/replica:0/task:0/device:CPU:0 [CPU]
```

If you built TensorFlow with GPU support you’ll see GPU entries as well.

### 7.6 Using `tf::BufferBuilder`  

Very handy when you need to assemble a protobuf manually:

```cpp
tf::BufferBuilder builder;
builder.AppendString("magic");
builder.AppendScalar<std::uint32_t>(12345);
builder.Append(std::vector<float>{0.1f, 0.2f, 0.3f}.data(),
               3 * sizeof(float));

tf::Buffer buf = builder.Build();   // buf now owns the memory
// buf can be passed to any TensorFlow C API that expects TF_Buffer*
```

### 7.7 Loading a SavedModel  

```cpp
// Assume you have a SavedModel directory exported from Python.
tf::SavedModel model("my_saved_model_dir", {"serve"}); // throws on error

// The Session is ready to run.
tf::Session<> sess = model.session();   // copy of the underlying session (move‑only)
tf::Graph<>    graph = model.graph();   // the graph that the model owns

// Inspect the signature (optional) – you can read the TF_MetaGraphDef
const TF_MetaGraphDef* meta = model.meta_graph_def();
if (meta) {
    // … use protobuf API to inspect inputs/outputs …
}

// Run the model (example: the SavedModel expects input "input_tensor")
std::vector<float> img(1*224*224*3, 0.5f);
tf::Tensor<> input = tf::Tensor<>::FromVector<float>({1,224,224,3}, img);

std::vector<tf::Feed> feeds = { {"input_tensor", 0, std::move(input)} };
std::vector<tf::Fetch> fetches = { {"output_tensor", 0} };

auto out = sess.Run(feeds, fetches);
```

---  

## Best Practices & Performance Tips  

| Guideline | Reason |
|-----------|--------|
| **Prefer `NoLock` for pure‑single‑threaded pipelines**. | Zero runtime overhead. |
| **Use `SharedMutex` for graphs that are read‑only after construction**. | Allows many readers without contention. |
| **Never share a `Tensor<>` across threads without a lock** (unless you only read). | The underlying TF_Tensor memory is not thread‑safe for concurrent writes. |
| **Reuse a `Session` object** for many runs instead of creating/destroying it each time. | Session creation involves graph‑copy/initialisation overhead. |
| **Avoid `TF_AllocateTensor` unless you need uninitialised memory** – `Tensor::FromVector` handles allocation and copying safely. |
| **When feeding large inputs, allocate once and reuse the buffer** (e.g., keep a pooled `Tensor<>`). | Reduces heap churn and improves cache locality. |
| **Enable compiler optimisations (`-O3` / `/O2`)** and *link with `-pthread`* on Linux for proper thread‑support. |
| **When debugging, set the environment variable `TF_CPP_MIN_LOG_LEVEL=0`** to get TensorFlow internal logs. |
| **Use `tf::ThrowIfNotOK` with a custom prefix** to pinpoint the failing call in stack traces. |

---  

## Troubleshooting & FAQ  

| Issue | Likely Cause | Fix |
|-------|--------------|-----|
| **Segmentation fault during `Run`** | Feeding a tensor with mismatched dtype or shape. | Verify `Tensor::dtype()` matches the placeholder’s dtype; check each dimension product equals the number of elements. |
| **`TF_GetCode` reports `TF_NOT_FOUND` for an op** | Misspelled operation name or wrong graph construction order. | Use `Graph::GetOperation` to check existence; ensure the op is added before you reference it. |
| **Performance drops dramatically when using Mutex** | You are calling `Run` from many threads *and* the graph does almost no work → lock contention dominates. | Move heavy computation to the graph (e.g., use larger models) or partition work across multiple sessions. |
| **Missing GPU devices** | TensorFlow C library compiled without GPU support or missing CUDA libraries. | Install `libtensorflow-gpu` or build TensorFlow from source with `--config=cuda`. Ensure `LD_LIBRARY_PATH` (Linux) or `PATH` (Windows) contains the CUDA runtime. |
| **`TF_NewSession` throws “Invalid argument”** | Passing a `SessionOptions` that contains an invalid protobuf. | If you use a custom config, verify it with `saved_model_cli` or protobuf text format. |
| **`TF_Buffer` memory leak** | Using `TF_NewBufferFromString` with a custom deallocator that does not free the memory. | Either let `tf::BufferBuilder` own the memory (it registers a proper deallocator) or make sure your custom deallocator frees the memory. |
| **`std::format` not found** | Compiler does not fully support C++20 `std::format`. | Use GCC ≥ 11, Clang ≥ 13, or MSVC ≥ 19.30. Alternatively, replace `std::format` calls with `std::stringstream` (the code will still compile). |

---  

## Extending the Wrapper  

* **Custom Ops** – Use `Graph::NewOperation` to create a node, then call `TF_SetAttr*` and `TF_AddInput` via the `OperationBuilder`.  
* **Profiling** – Add an overload of `Session::Run` that accepts a `TF_Buffer*` for `run_metadata`. After the run you can parse `run_metadata` with protobuf API.  
* **Partial Runs** – Implement a thin wrapper around `TF_SessionPartialRunSetup` and `TF_SessionRun` with a token.  
* **Alternative Error Model** – Replace the `ThrowIfNotOK` helper with a `std::expected`‑style return type (C++23) if you need a *no‑exception* API.  
* **Automatic Input/Output Signature Discovery** – Add a helper that walks `TF_GraphOperationByName` and extracts placeholder names and dtypes to auto‑populate `Feed`/`Fetch` vectors.  

---  

## Building & Running the Test Suite  

The repository ships a **Boost.Test** suite (`test_tf_cpp20.cpp`).  

### 1. Add Boost.Test (header only)  

```bash
# Ubuntu example
sudo apt-get install libboost-test-dev
```

### 2. Compile the tests  

```bash
g++ -std=c++20 -I./include -I/path/to/tensorflow/include \
    -I/path/to/boost/include test_tf_cpp20.cpp -o tf_tests \
    -L/path/to/tensorflow/lib -ltensorflow -pthread
```

### 3. Run  

```bash
./tf_tests
```

You should see output similar to:

```
Running 19 test cases...
*** No errors detected
```

All tests cover:

* Tensor construction, move semantics, type safety.  
* Graph building, operation lookup, `GetOperation` optional return.  
* Session creation, run, error handling.  
* Thread‑safe session runs (`Mutex` policy).  
* Shared‑read access to a graph (`SharedMutex`).  
* Device enumeration.  

Feel free to add more tests for your own extensions.

---  

## Summary  

The **TensorFlow‑C++20 wrapper** gives you:

* **RAII resource management** for every TensorFlow opaque handle.  
* **Compile‑time safety** via concepts and `std::span`.  
* **Zero‑overhead thread safety** through interchangeable lock policies.  
* **Rich, source‑location‑aware error messages** with `std::format`.  

All of this is delivered as a **single‑header, header‑only** library, making integration trivial.  
Follow the examples, select the appropriate lock policy, and you are ready to write high‑performance, thread‑safe TensorFlow inference code in modern C++. Happy coding!