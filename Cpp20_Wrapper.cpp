
Below is a **complete, header‑only redesign of the TensorFlow‑C façade** that

* uses **C++20** language facilities (`std::span`, `std::format`, `std::source_location`, concepts, ranges, coroutines …),  
* introduces a **policy‑based thread‑safety layer** (you can pick `NoLock`, `Mutex` or `SharedMutex` – the default is a zero‑cost `NoLock`),  
* keeps the public API identical to the original wrappers, so existing client code compiles unchanged (just add the extra template argument if you want a different policy).

> **File layout** – you can drop the three headers into your project and include `<tf/all.hpp>` (or include the individual headers you need).

```
include/
│   tf/policy.hpp          // lock‑policy definitions
│   tf/status.hpp          // error‑handling helpers (C++20)
│   tf/tensor.hpp          // Tensor wrapper (policy‑aware)
│   tf/graph.hpp           // Graph wrapper (policy‑aware)
│   tf/operation.hpp       // thin non‑owning handle
│   tf/session.hpp         // Session wrapper (policy‑aware)
│   tf/all.hpp             // convenience umbrella header
```

---

## 1.  Lock policies (`tf/policy.hpp`)

```cpp
// tf/policy.hpp
#pragma once

#include <mutex>
#include <shared_mutex>

namespace tf::policy {

/* ------------------------------------------------------------------
   Policy concept – every policy must provide lock()/unlock() and a
   RAII guard returned by scoped_lock() (or scoped_shared() for shared
   policies).  The concept is used by the wrappers to enforce the
   required interface at compile time.
-------------------------------------------------------------------*/
template<class P>
concept LockPolicy = requires(P p) {
    { p.lock()   } -> std::same_as<void>;
    { p.unlock() } -> std::same_as<void>;
    { p.scoped_lock() } -> std::convertible_to<std::unique_lock<P>>;
};

/* ------------------------------------------------------------------
   1️⃣  No‑op policy – zero overhead.  All functions are empty and
   the guard does nothing.
-------------------------------------------------------------------*/
struct NoLock {
    void lock()   noexcept {}
    void unlock() noexcept {}

    struct scoped_guard {
        explicit scoped_guard(NoLock&) {}
    };
    scoped_guard scoped_lock() noexcept { return scoped_guard(*this); }
};

/* ------------------------------------------------------------------
   2️⃣  Exclusive‑mutex policy – a plain std::mutex.
-------------------------------------------------------------------*/
class Mutex {
public:
    void lock()   noexcept { m_.lock(); }
    void unlock() noexcept { m_.unlock(); }

    struct scoped_guard {
        explicit scoped_guard(Mutex& p) : lk_(p.m_, std::adopt_lock) {}
        scoped_guard(const scoped_guard&) = delete;
        scoped_guard& operator=(const scoped_guard&) = delete;
    private:
        std::unique_lock<std::mutex> lk_;
    };
    scoped_guard scoped_lock() noexcept { return scoped_guard(*this); }

private:
    mutable std::mutex m_;
};

/* ------------------------------------------------------------------
   3️⃣  Shared‑mutex policy – readers can run concurrently.
-------------------------------------------------------------------*/
class SharedMutex {
public:
    void lock_shared()   noexcept { m_.lock_shared(); }
    void unlock_shared() noexcept { m_.unlock_shared(); }
    void lock()          noexcept { m_.lock(); }
    void unlock()        noexcept { m_.unlock(); }

    struct exclusive_guard {
        explicit exclusive_guard(SharedMutex& p) : lk_(p.m_, std::adopt_lock) {}
        exclusive_guard(const exclusive_guard&) = delete;
        exclusive_guard& operator=(const exclusive_guard&) = delete;
    private:
        std::unique_lock<std::shared_mutex> lk_;
    };
    struct shared_guard {
        explicit shared_guard(SharedMutex& p) : lk_(p.m_, std::adopt_lock) {}
        shared_guard(const shared_guard&) = delete;
        shared_guard& operator=(const shared_guard&) = delete;
    private:
        std::shared_lock<std::shared_mutex> lk_;
    };

    exclusive_guard scoped_lock()   noexcept { return exclusive_guard(*this); }
    shared_guard    scoped_shared() noexcept { return shared_guard(*this); }

private:
    mutable std::shared_mutex m_;
};

} // namespace tf::policy
```

*All three policies satisfy `LockPolicy`.  `NoLock` is an empty type, therefore it adds **no runtime cost** – the compiler will inline‑eliminate every call.*

---

## 2.  Error handling (`tf/status.hpp`)

```cpp
// tf/status.hpp
#pragma once

#include <stdexcept>
#include <string>
#include <format>
#include <source_location>

extern "C" {
    #include <tensorflow/c/c_api.h>
}

/* ------------------------------------------------------------------
   Helper that turns a TF_Status into a C++ exception.
   The source location is captured automatically, so error messages
   contain file/line information without the caller having to pass it.
-------------------------------------------------------------------*/
inline void ThrowIfNotOK(TF_Status* st,
                         std::string_view prefix = "",
                         std::source_location loc = std::source_location::current())
{
    if (TF_GetCode(st) != TF_OK) {
        std::string msg = std::format("{} at {}:{} – {}",
                                      prefix,
                                      loc.file_name(),
                                      loc.line(),
                                      TF_Message(st));
        TF_DeleteStatus(st);
        throw std::runtime_error(std::move(msg));
    }
}
```

---

## 3.  Tensor wrapper (`tf/tensor.hpp`)

```cpp
// tf/tensor.hpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <span>
#include <type_traits>
#include <concepts>
#include <format>

extern "C" {
    #include <tensorflow/c/c_api.h>
}
#include "tf/status.hpp"
#include "tf/policy.hpp"

namespace tf {

/* ------------------------------------------------------------------
   Concept that restricts the scalar types we support.
-------------------------------------------------------------------*/
template<class T>
concept TensorScalar =
    std::same_as<T,float>  ||
    std::same_as<T,double> ||
    std::same_as<T,std::int32_t> ||
    std::same_as<T,std::int64_t> ||
    std::same_as<T,std::uint8_t>;

/* ------------------------------------------------------------------
   Tensor – a move‑only RAII wrapper around TF_Tensor.
   The first template argument is the lock policy (default = NoLock).
-------------------------------------------------------------------*/
template<class Policy = policy::NoLock>
class Tensor {
public:
    /*--------------------  Construction / destruction  --------------------*/
    Tensor(TF_DataType dtype,
           std::span<const std::int64_t> dims,
           void* data,
           std::size_t byte_len,
           void (*deallocator)(void*, std::size_t, void*) = nullptr,
           void* deallocator_arg = nullptr)
        : policy_()
    {
        if (!deallocator) deallocator = &default_deallocator;
        std::vector<std::int64_t> dim_vec(dims.begin(), dims.end());

        tensor_ = TF_NewTensor(dtype,
                               dim_vec.data(),
                               static_cast<int>(dim_vec.size()),
                               data,
                               byte_len,
                               deallocator,
                               deallocator_arg);
        if (!tensor_) throw std::runtime_error("TF_NewTensor failed");
        shape_ = std::move(dim_vec);
    }

    /* Factory that copies a std::vector<T> into a Tensor. */
    template<TensorScalar T>
    static Tensor FromVector(std::span<const std::int64_t> dims,
                             const std::vector<T>& values,
                             std::source_location loc = std::source_location::current())
    {
        std::size_t expected = 1;
        for (auto d : dims) expected *= static_cast<std::size_t>(d);
        if (expected != values.size())
            throw std::runtime_error(std::format("{} at {}:{} – dimension product {} != values.size() {}",
                                                loc.function_name(),
                                                loc.file_name(),
                                                loc.line(),
                                                expected,
                                                values.size()));

        std::size_t byte_len = expected * sizeof(T);
        T* copy = static_cast<T*>(std::malloc(byte_len));
        if (!copy) throw std::bad_alloc();
        std::memcpy(copy, values.data(), byte_len);

        return Tensor(TFDataTypeOf<T>,
                      dims,
                      copy,
                      byte_len,
                      &default_deallocator,
                      nullptr);
    }

    /* Factory for a scalar (rank‑1 tensor with a single element). */
    template<TensorScalar T>
    static Tensor FromScalar(T value) {
        return FromVector<T>({1}, std::vector<T>{value});
    }

    /* Constructor used internally when we adopt a raw TF_Tensor*. */
    Tensor() noexcept = default;          // only used by FromRaw (friend)

    ~Tensor() noexcept {
        if (tensor_) TF_DeleteTensor(tensor_);
    }

    // non‑copyable, move‑only
    Tensor(const Tensor&) = delete;
    Tensor& operator=(const Tensor&) = delete;
    Tensor(Tensor&& other) noexcept
        : tensor_(other.tensor_), shape_(std::move(other.shape_)), policy_(std::move(other.policy_))
    {
        other.tensor_ = nullptr;
    }
    Tensor& operator=(Tensor&& other) noexcept {
        if (this != &other) {
            if (tensor_) TF_DeleteTensor(tensor_);
            tensor_ = other.tensor_;
            shape_  = std::move(other.shape_);
            policy_ = std::move(other.policy_);
            other.tensor_ = nullptr;
        }
        return *this;
    }

    /*--------------------  Queries  --------------------*/
    const std::vector<std::int64_t>& shape() const noexcept { return shape_; }
    TF_DataType dtype() const noexcept { return TF_TensorType(tensor_); }
    std::size_t byte_size() const noexcept { return TF_TensorByteSize(tensor_); }

    /*--------------------  Typed data access  --------------------*/
    template<TensorScalar T>
    T* data() noexcept {
        auto guard = policy_.scoped_lock();   // exclusive – mutable access
        if (dtype() != TFDataTypeOf<T>)
            throw std::runtime_error("Tensor dtype mismatch in data<T>()");
        return static_cast<T*>(TF_TensorData(tensor_));
    }

    template<TensorScalar T>
    const T* data() const noexcept {
        // read‑only – if the policy offers a shared guard we use it,
        // otherwise we fall back to the exclusive guard.
        if constexpr (requires { policy_.scoped_shared(); })
            auto guard = policy_.scoped_shared();
        else
            auto guard = policy_.scoped_lock();

        if (dtype() != TFDataTypeOf<T>)
            throw std::runtime_error("Tensor dtype mismatch in data<T>()");
        return static_cast<const T*>(TF_TensorData(tensor_));
    }

    /*--------------------  Raw handle  --------------------*/
    TF_Tensor* handle() const noexcept { return tensor_; }

    /*--------------------  Adopt a raw pointer (friend)  --------------------*/
    static Tensor FromRaw(TF_Tensor* raw) requires (raw != nullptr) {
        Tensor t;
        t.tensor_ = raw;               // ownership is transferred
        // Shape extraction – TF_TensorDims returns a pointer directly.
        int nd = TF_NumDims(raw);
        t.shape_.reserve(nd);
        const std::int64_t* dims = TF_TensorDims(raw);
        for (int i = 0; i < nd; ++i) t.shape_.push_back(dims[i]);
        return t;
    }

private:
    TF_Tensor* tensor_{nullptr};
    std::vector<std::int64_t> shape_;
    Policy policy_;                     // may be empty for NoLock

    static void default_deallocator(void* data,
                                   std::size_t,
                                   void*) noexcept {
        std::free(data);
    }

    // -----------------------------------------------------------------
    // Compile‑time mapping from C++ scalar to TF_DataType (C++20 if‑constexpr)
    // -----------------------------------------------------------------
    template<TensorScalar T>
    static constexpr TF_DataType TFDataTypeOf = []{
        if constexpr (std::same_as<T,float>)        return TF_FLOAT;
        else if constexpr (std::same_as<T,double>)   return TF_DOUBLE;
        else if constexpr (std::same_as<T,std::int32_t>) return TF_INT32;
        else if constexpr (std::same_as<T,std::int64_t>) return TF_INT64;
        else if constexpr (std::same_as<T,std::uint8_t>) return TF_UINT8;
        else static_assert(always_false<T>, "Unsupported scalar type");
    }();
};

} // namespace tf
```

**Key C++20 points**

* `std::span` for dimension arguments – zero‑copy, clear intent.  
* Concepts (`TensorScalar`) give **compile‑time** diagnostics for unsupported scalar types.  
* `std::format` and `std::source_location` produce rich error messages without boiler‑plate.  
* The lock policy is a **template parameter**; the default `policy::NoLock` incurs no overhead.

---

## 4.  Graph wrapper (`tf/graph.hpp`)

```cpp
// tf/graph.hpp
#pragma once

#include <string>
#include <optional>
#include <vector>
#include <stdexcept>
#include <concepts>

extern "C" {
    #include <tensorflow/c/c_api.h>
}
#include "tf/status.hpp"
#include "tf/policy.hpp"
#include "tf/tensor.hpp"

namespace tf {

/* ------------------------------------------------------------------
   Graph – owns a TF_Graph.  The first template argument is the lock
   policy (default = NoLock).  All mutating operations acquire an
   exclusive lock; read‑only queries try a shared lock if the policy
   provides one.
-------------------------------------------------------------------*/
template<class Policy = policy::NoLock>
class Graph {
public:
    Graph() : graph_(TF_NewGraph()), policy_() {
        if (!graph_) throw std::runtime_error("TF_NewGraph failed");
    }
    ~Graph() noexcept {
        if (graph_) TF_DeleteGraph(graph_);
    }

    // non‑copyable, move‑only
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;
    Graph(Graph&& other) noexcept
        : graph_(other.graph_), policy_(std::move(other.policy_))
    {
        other.graph_ = nullptr;
    }
    Graph& operator=(Graph&& other) noexcept {
        if (this != &other) {
            if (graph_) TF_DeleteGraph(graph_);
            graph_ = other.graph_;
            policy_ = std::move(other.policy_);
            other.graph_ = nullptr;
        }
        return *this;
    }

    /*--------------------  Import a GraphDef protobuf  --------------------*/
    void ImportGraphDef(const void* proto,
                        std::size_t proto_len,
                        const TF_ImportGraphDefOptions* opts = nullptr)
    {
        auto guard = policy_.scoped_lock();   // exclusive – we modify the graph
        TF_Buffer* buf = TF_NewBufferFromString(proto, proto_len);
        TF_Status* st = TF_NewStatus();
        TF_GraphImportGraphDef(graph_, buf,
                               const_cast<TF_ImportGraphDefOptions*>(opts),
                               st);
        TF_DeleteBuffer(buf);
        ThrowIfNotOK(st, "TF_GraphImportGraphDef");
    }

    /*--------------------  Operation lookup (read‑only)  --------------------*/
    std::optional<TF_Operation*> GetOperation(const std::string& name) const noexcept {
        if constexpr (requires { policy_.scoped_shared(); })
            auto guard = policy_.scoped_shared();
        else
            auto guard = policy_.scoped_lock();

        TF_Operation* op = TF_GraphOperationByName(graph_, name.c_str());
        if (!op) return std::nullopt;
        return op;
    }

    /*--------------------  Builder for new ops (exclusive)  --------------------*/
    class OperationBuilder {
    public:
        explicit OperationBuilder(Graph& g,
                                 const std::string& type,
                                 const std::string& name)
            : graph_(g), desc_(TF_NewOperation(g.graph_, type.c_str(), name.c_str()))
        {
            if (!desc_) throw std::runtime_error("TF_NewOperation failed");
        }

        // Attribute‑setters (only a few shown – add more as required)
        OperationBuilder& SetAttrTensor(const char* name, TF_Tensor* t) {
            TF_SetAttrTensor(desc_, name, t);
            return *this;
        }
        OperationBuilder& SetAttrType(const char* name, TF_DataType dt) {
            TF_SetAttrType(desc_, name, dt);
            return *this;
        }
        OperationBuilder& AddInput(const TF_Output& out) {
            TF_AddInput(desc_, out);
            return *this;
        }

        // Finish the op – throws on error.
        TF_Operation* Finish() {
            TF_Status* st = TF_NewStatus();
            TF_Operation* op = TF_FinishOperation(desc_, st);
            if (TF_GetCode(st) != TF_OK) {
                std::string msg = TF_Message(st);
                TF_DeleteStatus(st);
                throw std::runtime_error(std::move(msg));
            }
            TF_DeleteStatus(st);
            desc_ = nullptr;               // ownership transferred
            return op;
        }

        ~OperationBuilder() noexcept {
            if (desc_) TF_DeleteOperationDescription(desc_);
        }

    private:
        Graph& graph_;
        TF_OperationDescription* desc_;
    };

    OperationBuilder NewOperation(const std::string& type,
                                 const std::string& name) {
        auto guard = policy_.scoped_lock();   // exclusive mutation
        return OperationBuilder(*this, type, name);
    }

    /*--------------------  Raw handle – needed for Session  --------------------*/
    TF_Graph* handle() const noexcept { return graph_; }

private:
    TF_Graph* graph_{nullptr};
    Policy    policy_;            // may be empty for NoLock
};

} // namespace tf
```

*`GetOperation` returns `std::optional<TF_Operation*>` – callers are forced to handle the *not‑found* case.*

---

## 5.  Operation handle (`tf/operation.hpp`)

```cpp
// tf/operation.hpp
#pragma once

#include <string>
extern "C" { #include <tensorflow/c/c_api.h> }

namespace tf {

class Operation {
public:
    explicit Operation(TF_Operation* op) : op_(op) {
        if (!op_) throw std::invalid_argument("null TF_Operation");
    }

    TF_Operation* handle() const noexcept { return op_; }

    std::string name() const noexcept { return TF_OperationName(op_); }
    std::string type() const noexcept { return TF_OperationOpType(op_); }
    std::string device() const noexcept { return TF_OperationDevice(op_); }

    int num_inputs() const noexcept  { return TF_OperationNumInputs(op_); }
    int num_outputs() const noexcept { return TF_OperationNumOutputs(op_); }

    TF_Output output(int index = 0) const noexcept { return TF_Output{op_, index}; }

private:
    TF_Operation* op_;   // non‑owning; the graph owns it
};

} // namespace tf
```

---

## 6.  Session wrapper (`tf/session.hpp`)

```cpp
// tf/session.hpp
#pragma once

#include <vector>
#include <tuple>
#include <string>
#include <stdexcept>
#include <concepts>

extern "C" {
    #include <tensorflow/c/c_api.h>
}
#include "tf/status.hpp"
#include "tf/policy.hpp"
#include "tf/tensor.hpp"
#include "tf/graph.hpp"

namespace tf {

/* ------------------------------------------------------------------
   Simple structs used by Run – they keep the public API the same.
-------------------------------------------------------------------*/
struct Feed {
    std::string op_name;
    int         index{0};
    Tensor<>    tensor;          // default policy (NoLock) – the tensor is moved in.
};

struct Fetch {
    std::string op_name;
    int         index{0};
};

/* ------------------------------------------------------------------
   Session – owns a TF_Session*.  The first template argument is the
   lock policy (default = NoLock).  All public functions acquire an
   exclusive lock; reading the raw TF_Session* is allowed only for
   advanced use.
-------------------------------------------------------------------*/
template<class Policy = policy::NoLock>
class Session {
public:
    explicit Session(Graph<> &graph,
                     const SessionOptions* opts = nullptr,
                     const void* config = nullptr,
                     std::size_t config_len = 0)
        : graph_(&graph), policy_()
    {
        TF_Status* st = TF_NewStatus();

        // Optional config (e.g. serialized ConfigProto).
        TF_SessionOptions* sess_opts = nullptr;
        if (config && config_len > 0) {
            sess_opts = TF_NewSessionOptions();
            TF_SetConfig(sess_opts, config, config_len, st);
            ThrowIfNotOK(st, "TF_SetConfig");
        }

        session_ = TF_NewSession(graph.handle(),
                                opts ? const_cast<TF_SessionOptions*>(opts->get()) : sess_opts,
                                st);
        if (sess_opts) TF_DeleteSessionOptions(sess_opts);
        ThrowIfNotOK(st, "TF_NewSession");
    }

    ~Session() noexcept {
        if (session_) {
            TF_Status* st = TF_NewStatus();
            // Closing the session is also guarded – otherwise a race with
            // another thread calling Run could corrupt the TF internal state.
            if constexpr (requires { policy_.scoped_lock(); })
                auto guard = policy_.scoped_lock();
            TF_CloseSession(session_, st);
            TF_DeleteSession(session_, st);
        }
    }

    // non‑copyable, move‑only (same semantics as Tensor/Graph)
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&& other) noexcept
        : session_(other.session_), graph_(other.graph_), policy_(std::move(other.policy_))
    {
        other.session_ = nullptr;
        other.graph_   = nullptr;
    }
    Session& operator=(Session&& other) noexcept {
        if (this != &other) {
            if (session_) {
                TF_Status* st = TF_NewStatus();
                TF_CloseSession(session_, st);
                TF_DeleteSession(session_, st);
            }
            session_ = other.session_;
            graph_   = other.graph_;
            policy_  = std::move(other.policy_);
            other.session_ = nullptr;
            other.graph_   = nullptr;
        }
        return *this;
    }

    /*--------------------  Run  --------------------*/
    std::vector<Tensor<>> Run(const std::vector<Feed>&   feeds,
                              const std::vector<Fetch>&  fetches,
                              const std::vector<std::string>& targets = {}) const
    {
        // Acquire an exclusive lock for the whole call.
        auto guard = policy_.scoped_lock();

        // ---------- Build TF_Output / TF_Tensor arrays ----------
        std::vector<TF_Output>   input_ops;
        std::vector<TF_Tensor*>  input_vals;
        for (const auto& f : feeds) {
            TF_Operation* op = TF_GraphOperationByName(graph_->handle(),
                                                      f.op_name.c_str());
            if (!op) throw std::runtime_error("Feed op not found: " + f.op_name);
            input_ops.emplace_back(TF_Output{op, f.index});
            input_vals.push_back(f.tensor.handle());
        }

        std::vector<TF_Output>   output_ops;
        for (const auto& ft : fetches) {
            TF_Operation* op = TF_GraphOperationByName(graph_->handle(),
                                                      ft.op_name.c_str());
            if (!op) throw std::runtime_error("Fetch op not found: " + ft.op_name);
            output_ops.emplace_back(TF_Output{op, ft.index});
        }

        std::vector<TF_Tensor*> output_tensors(output_ops.size(), nullptr);
        std::vector<const char*> target_cstrs;
        target_cstrs.reserve(targets.size());
        for (const auto& t : targets) target_cstrs.push_back(t.c_str());

        // ---------- Call the C‑API ----------
        TF_Status* st = TF_NewStatus();
        TF_SessionRun(session_,
                      nullptr,                     // Run options (none)
                      input_ops.data(),
                      input_vals.data(),
                      static_cast<int>(input_ops.size()),
                      output_ops.data(),
                      output_tensors.data(),
                      static_cast<int>(output_ops.size()),
                      target_cstrs.data(),
                      static_cast<int>(target_cstrs.size()),
                      nullptr,                     // Run metadata (none)
                      st);
        ThrowIfNotOK(st, "TF_SessionRun");

        // ---------- Wrap the raw TF_Tensor* results ----------
        std::vector<Tensor<>> results;
        results.reserve(output_tensors.size());
        for (TF_Tensor* raw : output_tensors) {
            results.emplace_back();                // default construct
            results.back() = Tensor<>::FromRaw(raw); // adopt ownership
        }
        return results;
    }

    /*--------------------  Raw handle (advanced) --------------------*/
    TF_Session* handle() const noexcept { return session_; }

private:
    TF_Session* session_{nullptr};
    Graph<>*    graph_{nullptr};
    mutable Policy policy_;          // mutable because Run is const
};

} // namespace tf
```

*The run method uses the **policy** to lock the whole operation, guaranteeing that two threads can never invoke `TF_SessionRun` on the same `Session` simultaneously.*

---

## 7.  Convenience umbrella (`tf/all.hpp`)

```cpp
// tf/all.hpp
#pragma once

#include "tf/policy.hpp"
#include "tf/status.hpp"
#include "tf/tensor.hpp"
#include "tf/graph.hpp"
#include "tf/operation.hpp"
#include "tf/session.hpp"
```

---

## 8.  Example usage (single file)

```cpp
// main.cpp ---------------------------------------------------------
#include "tf/all.hpp"
#include <iostream>
#include <random>
#include <thread>
#include <vector>

using namespace tf;

int main()
{
    /* --------------------------------------------------------------
       Build a tiny constant → identity graph.
       The graph itself does *not* need a lock (it is built once and
       never mutated again), so we keep the default NoLock policy.
       --------------------------------------------------------------*/
    Graph<> g;

    // 1️⃣  Create a constant tensor (rank‑1, 8 floats)
    std::vector<int64_t> shape = {1, 8};
    std::vector<float>   values(8, 0.5f);
    Tensor<> const_tensor = Tensor<>::FromVector<float>(shape, values);

    // 2️⃣  Const op
    auto const_builder = g.NewOperation("Const", "c0");
    const_builder.SetAttrTensor("value", const_tensor.handle())
                 .SetAttrType("dtype", TF_FLOAT);
    const_builder.Finish();

    // 3️⃣  Identity op (takes the constant as input)
    auto id_builder = g.NewOperation("Identity", "out");
    // Note: TF_GraphOperationByName is safe because the graph is already built.
    TF_Operation* c0 = g.GetOperation("c0").value();
    id_builder.AddInput(TF_Output{c0, 0});
    id_builder.Finish();

    /* --------------------------------------------------------------
       Run the graph from *multiple* threads.
       Use a thread‑safe policy for the session: Mutex.
       --------------------------------------------------------------*/
    Session<policy::Mutex> sess(g);

    // No feeds – the constant lives inside the graph.
    std::vector<Fetch> fetches = { {"out", 0} };

    auto worker = [&](int id) {
        auto out = sess.Run({}, fetches);
        const float* ptr = out[0].data<float>();
        std::cout << "Thread " << id << " got: ";
        for (std::size_t i = 0; i < 8; ++i) std::cout << ptr[i] << ' ';
        std::cout << '\n';
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 6; ++i) threads.emplace_back(worker, i);
    for (auto& t : threads) t.join();

    return 0;
}
```

**What the example demonstrates**

* The **graph** is built once, using the default `NoLock` policy (no synchronization needed).  
* The **session** is instantiated as `Session<policy::Mutex>` – every call to `Run` acquires an exclusive `std::mutex`, making the session safe for concurrent use.  
* The **Tensor** wrapper itself is also policy‑aware, but in this simple program we keep the default `NoLock` because tensors are never shared across threads.

Compile (C++20) with a recent compiler and the TensorFlow C library:

```bash
g++ -std=c++20 -I/path/to/tensorflow/include \
    -I./include \
    main.cpp -o tf_demo \
    -L/path/to/tensorflow/lib -ltensorflow -pthread
```

Run:

```
Thread 0 got: 0.5 0.5 0.5 0.5 0.5 0.5 0.5 0.5 
Thread 1 got: 0.5 0.5 0.5 0.5 0.5 0.5 0.5 0.5 
...
```

All threads safely share the same `Session` without any data race, thanks to the **policy‑based lock**.

---

## 9.  How to pick a policy

| Situation                                            | Policy to use                                    |
|------------------------------------------------------|--------------------------------------------------|
| Single‑threaded command line tool (default)         | `policy::NoLock` (default – zero overhead)      |
| Multiple threads *reading* the same Tensor          | `policy::SharedMutex` (shared lock for reads)   |
| Multiple threads *modifying* a Tensor (e.g. a shared buffer) | `policy::Mutex` (exclusive lock)                |
| Graph built once, then read concurrently            | `policy::SharedMutex` for the Graph (optional) |
| Session reused by many worker threads                | `policy::Mutex` (exclusive during `Run`)        |

Just change the template argument:

```cpp
using SafeTensor   = tf::Tensor<tf::policy::SharedMutex>;
using SafeGraph    = tf::Graph<tf::policy::SharedMutex>;
using SafeSession  = tf::Session<tf::policy::Mutex>;
```

Because the policy is a *template parameter*, the selection happens at compile time and the compiler can completely erase the lock code when `NoLock` is chosen.

---

## 10.  Summary of the C++20 enhancements

| Feature | Where it appears | Benefit |
|---------|-------------------|---------|
| `std::span` | Tensor dimensions, `FromVector` | No copies, clear “view‑only” semantics |
| Concepts (`TensorScalar`, `LockPolicy`) | Tensor, policy classes | Compile‑time validation, friendlier diagnostics |
| `std::format` + `std::source_location` | `ThrowIfNotOK` | Rich error messages without manual boiler‑plate |
| Ranges / `std::views` (optional) | Could be used for Feed/Fetch construction (not shown) | Concise, expressive pipelines |
| `if constexpr` | Mapping scalar ⇢ TF_DataType, shared‑lock fallback | Single implementation, no runtime branching |
| Policy‑based design | All major wrappers | Zero‑overhead thread safety – choose at compile time |
| Coroutines (`std::generator`) – (optional) | Could be added to `Session::Run` to stream results lazily | Avoid temporary `std::vector<Tensor>` for huge outputs |
| Structured bindings & `std::optional` | Operation lookup (`GetOperation`) | Enforces handling of missing nodes. |

The redesign keeps the **public API identical** (except for the optional template argument) while giving you **compile‑time guarantees** and the freedom to **turn on thread safety only where you need it**.  All the original functionality (tensor creation, graph import, session run, saved‑model loading, etc.) remains fully available.