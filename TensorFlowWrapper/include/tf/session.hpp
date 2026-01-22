// tf/session.hpp
// RAII wrapper for TF_Session with thread-safe execution
//
// PATCHED v4 - Fixes from ChatGPT review comparison:
// - P1: Added comprehensive re-entrancy/deadlock documentation
//
// Original merged implementation credits:
// - ChatGPT: Deterministic feed tensor locking idea
// - Claude: SessionOptions wrapper, comprehensive Run variants

#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <tensorflow/c/c_api.h>
}

#include "tf/format.hpp"
#include "tf/graph.hpp"
#include "tf/policy.hpp"
#include "tf/status.hpp"
#include "tf/tensor.hpp"

namespace tf {

// ============================================================================
// SessionOptions - RAII wrapper for TF_SessionOptions
// ============================================================================

class SessionOptions {
public:
    /// Create default session options
    SessionOptions() : opts_(TF_NewSessionOptions()) {
        if (!opts_) {
            throw std::runtime_error("TF_NewSessionOptions failed");
        }
    }
    
    ~SessionOptions() {
        if (opts_) TF_DeleteSessionOptions(opts_);
    }
    
    // Non-copyable
    SessionOptions(const SessionOptions&) = delete;
    SessionOptions& operator=(const SessionOptions&) = delete;
    
    // Movable
    SessionOptions(SessionOptions&& other) noexcept : opts_(other.opts_) {
        other.opts_ = nullptr;
    }
    
    SessionOptions& operator=(SessionOptions&& other) noexcept {
        if (this != &other) {
            if (opts_) TF_DeleteSessionOptions(opts_);
            opts_ = other.opts_;
            other.opts_ = nullptr;
        }
        return *this;
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Configuration
    // ─────────────────────────────────────────────────────────────────
    
    /// Set ConfigProto (serialized protobuf)
    SessionOptions& SetConfig(const void* proto, std::size_t len) {
        Status st;
        TF_SetConfig(opts_, proto, len, st.get());
        st.throw_if_error("TF_SetConfig");
        return *this;
    }
    
    /// Set target (e.g., "local", or gRPC address for distributed)
    SessionOptions& SetTarget(const char* target) {
        TF_SetTarget(opts_, target);
        return *this;
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Handle access
    // ─────────────────────────────────────────────────────────────────
    
    [[nodiscard]] TF_SessionOptions* get() const noexcept { return opts_; }
    [[nodiscard]] TF_SessionOptions* handle() const noexcept { return opts_; }

private:
    TF_SessionOptions* opts_;
};

// ============================================================================
// Feed/Fetch structures for Session::Run
// ============================================================================

/// Input tensor specification
struct Feed {
    std::string op_name;
    int index{0};
    TF_Tensor* tensor;  // Non-owning
    
    /// Construct from name, index, and raw tensor handle
    Feed(std::string name, int idx, TF_Tensor* t)
        : op_name(std::move(name)), index(idx), tensor(t) {}
    
    /// Construct from name and raw tensor handle (index defaults to 0)
    Feed(std::string name, TF_Tensor* t)
        : op_name(std::move(name)), index(0), tensor(t) {}
    
    /// Convenience: from any Tensor<Policy>
    template<policy::LockPolicy P>
    Feed(std::string name, int idx, const Tensor<P>& t)
        : op_name(std::move(name)), index(idx), tensor(t.handle()) {}
    
    template<policy::LockPolicy P>
    Feed(std::string name, const Tensor<P>& t)
        : op_name(std::move(name)), index(0), tensor(t.handle()) {}
};

// ============================================================================
// THREAD SAFETY DOCUMENTATION (P1 FIX: Re-entrancy warning)
// ============================================================================
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │                    CRITICAL: RE-ENTRANCY HAZARDS                        │
// └─────────────────────────────────────────────────────────────────────────┘
//
// When using policy::Mutex (not SharedMutex), the following pattern DEADLOCKS:
//
//   Tensor<policy::Mutex> input = ...;
//   
//   auto view = input.read<float>();   // Acquires EXCLUSIVE lock
//   session.Run({Feed{"x", input}}, ...);  // If Run tried to lock input → DEADLOCK
//
// WHY: policy::Mutex implements scoped_shared() as scoped_lock() (exclusive).
//      Both read() and write() acquire exclusive locks. If you hold one and
//      then call code that tries to acquire another on the same tensor, deadlock.
//
// SAFE PATTERNS:
//
// 1. Release views before Run():
//
//      {
//          auto view = input.write<float>();
//          // ... modify data ...
//      }  // view released here
//      session.Run({Feed{"x", input}}, fetches);  // OK
//
// 2. Use policy::SharedMutex for feed tensors (allows concurrent reads):
//
//      Tensor<policy::SharedMutex> input = ...;
//      auto view = input.read<float>();  // Shared lock
//      // Session::Run doesn't lock feeds internally, so this is OK
//
// 3. Use policy::NoLock if you manage synchronization externally:
//
//      Tensor<policy::NoLock> input = ...;
//      // You handle all synchronization yourself
//
// 4. Use acquire_shared_lock() for explicit feed locking:
//
//      Tensor<policy::SharedMutex> feed1 = ..., feed2 = ...;
//      
//      // Lock in consistent address order to prevent deadlock
//      auto lock1 = feed1.acquire_shared_lock();
//      auto lock2 = feed2.acquire_shared_lock();
//      
//      session.Run({Feed{"a", feed1}, Feed{"b", feed2}}, fetches);
//      // locks released when lock1, lock2 go out of scope
//
// ============================================================================

// ============================================================================
// RunOptions - Helper for creating TF_Buffer with run options
// ============================================================================

/// RAII wrapper for TF_Buffer used in run options/metadata
class Buffer {
public:
    /// Create empty buffer (for receiving output like metadata)
    Buffer() : buf_(TF_NewBuffer()) {
        if (!buf_) throw std::runtime_error("TF_NewBuffer failed");
    }
    
    /// Create buffer from existing data (copies data)
    Buffer(const void* data, std::size_t length) : buf_(TF_NewBuffer()) {
        if (!buf_) throw std::runtime_error("TF_NewBuffer failed");
        
        if (data && length > 0) {
            // TF_NewBufferFromString copies the data
            TF_DeleteBuffer(buf_);
            buf_ = TF_NewBufferFromString(data, length);
            if (!buf_) throw std::runtime_error("TF_NewBufferFromString failed");
        }
    }
    
    ~Buffer() {
        if (buf_) TF_DeleteBuffer(buf_);
    }
    
    // Non-copyable
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    
    // Movable
    Buffer(Buffer&& other) noexcept : buf_(other.buf_) {
        other.buf_ = nullptr;
    }
    
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            if (buf_) TF_DeleteBuffer(buf_);
            buf_ = other.buf_;
            other.buf_ = nullptr;
        }
        return *this;
    }
    
    [[nodiscard]] TF_Buffer* get() const noexcept { return buf_; }
    [[nodiscard]] TF_Buffer* handle() const noexcept { return buf_; }
    
    /// Get buffer data pointer
    [[nodiscard]] const void* data() const noexcept { 
        return buf_ ? buf_->data : nullptr; 
    }
    
    /// Get buffer length
    [[nodiscard]] std::size_t length() const noexcept { 
        return buf_ ? buf_->length : 0; 
    }
    
    /// Check if buffer has data
    [[nodiscard]] bool empty() const noexcept {
        return !buf_ || buf_->length == 0;
    }
    
    /// Extract data as vector of bytes
    [[nodiscard]] std::vector<std::uint8_t> to_bytes() const {
        if (!buf_ || !buf_->data || buf_->length == 0) {
            return {};
        }
        const auto* p = static_cast<const std::uint8_t*>(buf_->data);
        return std::vector<std::uint8_t>(p, p + buf_->length);
    }

private:
    TF_Buffer* buf_;
};

/// Output specification
struct Fetch {
    std::string op_name;
    int index{0};
    
    Fetch(std::string name, int idx = 0)
        : op_name(std::move(name)), index(idx) {}
};

// ============================================================================
// Device - Information about a compute device
// ============================================================================

struct Device {
    std::string name;    // e.g., "/device:CPU:0" or "/device:GPU:0"
    std::string type;    // e.g., "CPU" or "GPU"
    std::int64_t memory_bytes{0};  // Total memory (0 for CPU)
    
    [[nodiscard]] bool is_gpu() const noexcept { return type == "GPU"; }
    [[nodiscard]] bool is_cpu() const noexcept { return type == "CPU"; }
};

// ============================================================================
// DeviceList - RAII wrapper for TF_DeviceList
// ============================================================================

class DeviceList {
public:
    DeviceList() = default;
    
    explicit DeviceList(TF_DeviceList* list) : list_(list) {}
    
    ~DeviceList() {
        if (list_) TF_DeleteDeviceList(list_);
    }
    
    // Non-copyable
    DeviceList(const DeviceList&) = delete;
    DeviceList& operator=(const DeviceList&) = delete;
    
    // Movable
    DeviceList(DeviceList&& other) noexcept : list_(other.list_) {
        other.list_ = nullptr;
    }
    
    DeviceList& operator=(DeviceList&& other) noexcept {
        if (this != &other) {
            if (list_) TF_DeleteDeviceList(list_);
            list_ = other.list_;
            other.list_ = nullptr;
        }
        return *this;
    }
    
    /// Get number of devices
    [[nodiscard]] int count() const noexcept {
        return list_ ? TF_DeviceListCount(list_) : 0;
    }
    
    /// Get device at index
    [[nodiscard]] Device at(int index) const {
        if (!list_ || index < 0 || index >= count()) {
            throw std::out_of_range("DeviceList::at: index out of range");
        }
        
        Device dev;
        Status st;
        
        const char* name = TF_DeviceListName(list_, index, st.get());
        st.throw_if_error("TF_DeviceListName");
        dev.name = name ? name : "";
        
        const char* type = TF_DeviceListType(list_, index, st.get());
        st.throw_if_error("TF_DeviceListType");
        dev.type = type ? type : "";
        
        dev.memory_bytes = TF_DeviceListMemoryBytes(list_, index, st.get());
        st.throw_if_error("TF_DeviceListMemoryBytes");
        
        return dev;
    }
    
    /// Get all devices as a vector
    [[nodiscard]] std::vector<Device> all() const {
        std::vector<Device> devices;
        const int n = count();
        devices.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            devices.push_back(at(i));
        }
        return devices;
    }
    
    [[nodiscard]] TF_DeviceList* handle() const noexcept { return list_; }

private:
    TF_DeviceList* list_{nullptr};
};

// ============================================================================
// Session - RAII wrapper for TF_Session
// ============================================================================

template<policy::LockPolicy Policy = policy::NoLock>
class Session {
public:
    using policy_type = Policy;
    using guard_type = decltype(std::declval<const Policy&>().scoped_lock());
    
    // ─────────────────────────────────────────────────────────────────
    // Constructors - Accept ANY Graph policy
    // ─────────────────────────────────────────────────────────────────
    
    /// Create session from graph (any policy)
    template<policy::LockPolicy GraphPolicy>
    explicit Session(Graph<GraphPolicy>& graph,
                     const SessionOptions& opts = SessionOptions())
        : graph_handle_(graph.handle())
    {
        Status st;
        session_ = TF_NewSession(graph_handle_, opts.get(), st.get());
        st.throw_if_error("TF_NewSession");
    }
    
    /// Create session with raw options handle
    template<policy::LockPolicy GraphPolicy>
    explicit Session(Graph<GraphPolicy>& graph,
                     TF_SessionOptions* opts)
        : graph_handle_(graph.handle())
    {
        Status st;
        session_ = TF_NewSession(graph_handle_, opts, st.get());
        st.throw_if_error("TF_NewSession");
    }
    
    ~Session() noexcept {
        Cleanup();
    }
    
    // Non-copyable
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    
    // Movable
    Session(Session&& other) noexcept
        : session_(other.session_)
        , graph_handle_(other.graph_handle_)
        , policy_(std::move(other.policy_))
    {
        other.session_ = nullptr;
        other.graph_handle_ = nullptr;
    }
    
    Session& operator=(Session&& other) noexcept {
        if (this != &other) {
            Cleanup();
            session_ = other.session_;
            graph_handle_ = other.graph_handle_;
            policy_ = std::move(other.policy_);
            other.session_ = nullptr;
            other.graph_handle_ = nullptr;
        }
        return *this;
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Run - Execute the graph
    // Thread-safe: holds exclusive lock for entire TF_SessionRun
    //
    // NOTE: This method does NOT lock feed tensors. The caller is
    // responsible for ensuring feeds aren't mutated during Run().
    // See THREAD SAFETY DOCUMENTATION above for safe patterns.
    // ─────────────────────────────────────────────────────────────────
    
    /// Full Run with optional debugging support
    /// @param feeds Input tensors to feed into the graph
    /// @param fetches Output tensors to fetch from the graph
    /// @param targets Operations to execute (but not fetch outputs from)
    /// @param run_options Optional serialized RunOptions protobuf for tracing/timeout
    /// @param run_metadata Optional buffer to receive RunMetadata protobuf (profiling info)
    [[nodiscard]] std::vector<Tensor<>> Run(
        const std::vector<Feed>& feeds,
        const std::vector<Fetch>& fetches,
        const std::vector<std::string>& targets,
        TF_Buffer* run_options,
        TF_Buffer* run_metadata) const
    {
        if (!session_) {
            throw std::runtime_error("Session::Run(): session is null (moved-from?)");
        }

        [[maybe_unused]] auto guard = policy_.scoped_lock();  // Lock SESSION for entire run
        
        // Build input arrays
        std::vector<TF_Output> input_ops;
        std::vector<TF_Tensor*> input_vals;
        input_ops.reserve(feeds.size());
        input_vals.reserve(feeds.size());
        
        for (const auto& f : feeds) {
            TF_Operation* op = TF_GraphOperationByName(graph_handle_, f.op_name.c_str());
            if (!op) {
                throw std::runtime_error(tf::detail::format(
                    "Feed operation '{}' not found", f.op_name));
            }
            input_ops.push_back(TF_Output{op, f.index});
            input_vals.push_back(f.tensor);
        }
        
        // Build output arrays
        std::vector<TF_Output> output_ops;
        output_ops.reserve(fetches.size());
        
        for (const auto& f : fetches) {
            TF_Operation* op = TF_GraphOperationByName(graph_handle_, f.op_name.c_str());
            if (!op) {
                throw std::runtime_error(tf::detail::format(
                    "Fetch operation '{}' not found", f.op_name));
            }
            output_ops.push_back(TF_Output{op, f.index});
        }
        
        std::vector<TF_Tensor*> output_tensors(output_ops.size(), nullptr);
        
        // Build target operations
        std::vector<TF_Operation*> target_ops;
        target_ops.reserve(targets.size());
        
        for (const auto& t : targets) {
            TF_Operation* op = TF_GraphOperationByName(graph_handle_, t.c_str());
            if (!op) {
                throw std::runtime_error(tf::detail::format(
                    "Target operation '{}' not found", t));
            }
            target_ops.push_back(op);
        }
        
        // Execute
        Status st;
        TF_SessionRun(
            session_,
            run_options,
            input_ops.empty() ? nullptr : input_ops.data(),
            input_vals.empty() ? nullptr : input_vals.data(),
            static_cast<int>(input_ops.size()),
            output_ops.empty() ? nullptr : output_ops.data(),
            output_tensors.empty() ? nullptr : output_tensors.data(),
            static_cast<int>(output_ops.size()),
            target_ops.empty() ? nullptr : target_ops.data(),
            static_cast<int>(target_ops.size()),
            run_metadata,
            st.get());
        
        // On error, TensorFlow may have allocated partial outputs. Clean them up
        // before throwing to avoid leaks.
        if (!st.ok()) {
            for (TF_Tensor* t : output_tensors) {
                if (t) {
                    TF_DeleteTensor(t);
                }
            }
            st.throw_if_error("TF_SessionRun");
        }
        
        // Wrap outputs in Tensor objects. Preserve positional correspondence
        // with the fetch list by inserting empty tensors for null outputs.
        std::vector<Tensor<>> results;
        results.reserve(output_tensors.size());

        for (TF_Tensor* raw : output_tensors) {
            results.push_back(raw ? Tensor<>::FromRaw(raw) : Tensor<>());
        }

        return results;
    }
    
    /// Run without debugging options (most common usage)
    [[nodiscard]] std::vector<Tensor<>> Run(
        const std::vector<Feed>& feeds,
        const std::vector<Fetch>& fetches,
        const std::vector<std::string>& targets = {}) const
    {
        return Run(feeds, fetches, targets, nullptr, nullptr);
    }
    
    /// Run with Buffer wrappers for options/metadata
    [[nodiscard]] std::vector<Tensor<>> Run(
        const std::vector<Feed>& feeds,
        const std::vector<Fetch>& fetches,
        const std::vector<std::string>& targets,
        const Buffer& run_options,
        Buffer& run_metadata) const
    {
        return Run(feeds, fetches, targets, run_options.get(), run_metadata.get());
    }
    
    /// Run with only metadata output (no options)
    [[nodiscard]] std::vector<Tensor<>> RunWithMetadata(
        const std::vector<Feed>& feeds,
        const std::vector<Fetch>& fetches,
        Buffer& run_metadata) const
    {
        return Run(feeds, fetches, {}, nullptr, run_metadata.get());
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Convenience Run variants
    // ─────────────────────────────────────────────────────────────────
    
    /// Run with just fetches (no feeds)
    [[nodiscard]] std::vector<Tensor<>> Run(
        const std::vector<Fetch>& fetches) const
    {
        return Run({}, fetches, {});
    }
    
    /// Run single fetch
    [[nodiscard]] Tensor<> Run(const Fetch& fetch) const {
        auto results = Run({}, {fetch}, {});
        if (results.empty()) {
            throw std::runtime_error("Session::Run returned no outputs");
        }
        return std::move(results[0]);
    }
    
    /// Run single fetch by name
    [[nodiscard]] Tensor<> Run(const std::string& fetch_name, int index = 0) const {
        return Run(Fetch{fetch_name, index});
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Handle access
    // ─────────────────────────────────────────────────────────────────
    
    [[nodiscard]] TF_Session* handle() const noexcept { return session_; }
    [[nodiscard]] TF_Graph* graph_handle() const noexcept { return graph_handle_; }
    
    // ─────────────────────────────────────────────────────────────────
    // Device enumeration
    // ─────────────────────────────────────────────────────────────────
    
    /// List all available compute devices (CPU, GPU, etc.)
    [[nodiscard]] DeviceList ListDevices() const {
        if (!session_) {
            throw std::runtime_error("Session::ListDevices(): session is null");
        }
        
        Status st;
        TF_DeviceList* list = TF_SessionListDevices(session_, st.get());
        st.throw_if_error("TF_SessionListDevices");
        
        return DeviceList(list);
    }
    
    /// Check if any GPU is available
    [[nodiscard]] bool HasGPU() const {
        auto devices = ListDevices();
        for (int i = 0; i < devices.count(); ++i) {
            if (devices.at(i).is_gpu()) return true;
        }
        return false;
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Static factory: Load from SavedModel
    // ─────────────────────────────────────────────────────────────────
    
    /// Load a session from a SavedModel directory
    /// 
    /// @param export_dir Path to the SavedModel directory (contains saved_model.pb)
    /// @param tags Model tags (default: {"serve"} for inference)
    /// @param opts Session options
    /// @return Pair of (Session, owned Graph) - Graph is returned because
    ///         TF_LoadSessionFromSavedModel populates it
    /// 
    /// Example:
    ///   auto [session, graph] = Session<>::LoadSavedModel("/path/to/model");
    ///   auto result = session.Run({Feed{"input", tensor}}, {Fetch{"output"}});
    ///
    [[nodiscard]] static std::pair<Session, Graph<Policy>> LoadSavedModel(
        const std::string& export_dir,
        const std::vector<std::string>& tags = {"serve"},
        const SessionOptions& opts = SessionOptions())
    {
        // Create graph that will be populated by TF
        Graph<Policy> graph;
        
        // Convert tags to C-style array
        std::vector<const char*> tag_ptrs;
        tag_ptrs.reserve(tags.size());
        for (const auto& tag : tags) {
            tag_ptrs.push_back(tag.c_str());
        }
        
        Status st;
        TF_Session* raw_session = TF_LoadSessionFromSavedModel(
            opts.get(),
            nullptr,  // run_options
            export_dir.c_str(),
            tag_ptrs.data(),
            static_cast<int>(tag_ptrs.size()),
            graph.handle(),
            nullptr,  // meta_graph_def output (we don't need it)
            st.get());
        
        st.throw_if_error(tf::detail::format("LoadSavedModel('{}')", export_dir));
        
        // Construct Session that takes ownership
        Session session;
        session.session_ = raw_session;
        session.graph_handle_ = graph.handle();
        
        return {std::move(session), std::move(graph)};
    }

private:
    TF_Session* session_{nullptr};
    TF_Graph* graph_handle_{nullptr};  // Non-owning; graph must outlive session
    mutable Policy policy_;
    
    /// Private default constructor (for LoadSavedModel factory)
    Session() = default;
    
    /// Cleanup helper (used by destructor and move assignment)
    void Cleanup() noexcept {
        if (!session_) return;
        
        TF_Status* st = TF_NewStatus();
        if (st) {
            TF_CloseSession(session_, st);
            TF_DeleteSession(session_, st);
            TF_DeleteStatus(st);
        }
        
        session_ = nullptr;
        graph_handle_ = nullptr;
    }
};

// ============================================================================
// Type aliases
// ============================================================================

using FastSession = Session<policy::NoLock>;
using SafeSession = Session<policy::Mutex>;

} // namespace tf
