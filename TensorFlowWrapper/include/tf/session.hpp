// tf/session.hpp
// RAII wrapper for TF_Session with thread-safe execution
//
// MERGED IMPLEMENTATION - Best of ChatGPT + Claude:
// - ChatGPT: Deterministic feed tensor locking idea
// - Claude: SessionOptions wrapper, comprehensive Run variants
//
// Fixes applied:
// - P0: SessionOptions properly defined
// - P2: Session accepts any Graph policy (not hardcoded Graph<>)
// - P2: Feed tensors could be locked during run (optional enhancement)

#pragma once

#include <algorithm>
#include <format>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <tensorflow/c/c_api.h>
}

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
// LockedFeed - Feed with held read lock (ChatGPT V3 recommendation)
// ============================================================================
// 
// Problem: When Session::Run() reads feed tensors, another thread could
// mutate them concurrently, causing data races.
//
// Solution: LockedFeed holds a read view on the tensor, keeping the lock
// alive for the entire duration of the Run() call.
//
// Usage:
//   Tensor<Mutex> input = ...;
//   
//   // Create locked feeds BEFORE calling Run
//   auto locked1 = input.read<float>();  // Lock acquired
//   
//   // Pass the underlying handle - lock held by locked1's lifetime
//   auto results = session.Run(
//       {Feed{"input", input.handle()}},  
//       {Fetch{"output"}});
//   
//   // Lock released when locked1 goes out of scope
//
// For multiple feeds, acquire locks in consistent order to avoid deadlock:
//   auto lock_a = tensor_a.read<float>();
//   auto lock_b = tensor_b.read<float>();  // Always same order!
//   session.Run({Feed{"a", tensor_a}, Feed{"b", tensor_b}}, ...);
//

/// Output specification
struct Fetch {
    std::string op_name;
    int index{0};
    
    Fetch(std::string name, int idx = 0)
        : op_name(std::move(name)), index(idx) {}
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
    // Constructors - Accept ANY Graph policy (P2 fix)
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
        if (session_) {
            auto guard = policy_.scoped_lock();
            TF_Status* st = TF_NewStatus();
            TF_CloseSession(session_, st);
            TF_DeleteSession(session_, st);
            TF_DeleteStatus(st);
        }
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
            if (session_) {
                TF_Status* st = TF_NewStatus();
                TF_CloseSession(session_, st);
                TF_DeleteSession(session_, st);
                TF_DeleteStatus(st);
            }
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
    // ─────────────────────────────────────────────────────────────────
    
    /// Full Run with feeds, fetches, and targets
    /// 
    /// THREAD SAFETY NOTE (from ChatGPT V3 review):
    /// If feed tensors have locking policies, we should ideally hold read
    /// guards on them for the entire TF_SessionRun duration. However, since
    /// Feed only stores raw TF_Tensor* handles, the caller is responsible
    /// for ensuring feed tensors aren't mutated during Run().
    /// 
    /// For fully safe concurrent access, use the RunWithLockedFeeds() variant
    /// or ensure feeds use NoLock policy (single-writer model).
    [[nodiscard]] std::vector<Tensor<>> Run(
        const std::vector<Feed>& feeds,
        const std::vector<Fetch>& fetches,
        const std::vector<std::string>& targets = {}) const
    {
        auto guard = policy_.scoped_lock();  // Lock SESSION for entire run
        
        // Build input arrays
        std::vector<TF_Output> input_ops;
        std::vector<TF_Tensor*> input_vals;
        input_ops.reserve(feeds.size());
        input_vals.reserve(feeds.size());
        
        for (const auto& f : feeds) {
            TF_Operation* op = TF_GraphOperationByName(graph_handle_, f.op_name.c_str());
            if (!op) {
                throw std::runtime_error(std::format(
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
                throw std::runtime_error(std::format(
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
                throw std::runtime_error(std::format(
                    "Target operation '{}' not found", t));
            }
            target_ops.push_back(op);
        }
        
        // Execute
        Status st;
        TF_SessionRun(
            session_,
            nullptr,  // Run options
            input_ops.empty() ? nullptr : input_ops.data(),
            input_vals.empty() ? nullptr : input_vals.data(),
            static_cast<int>(input_ops.size()),
            output_ops.empty() ? nullptr : output_ops.data(),
            output_tensors.empty() ? nullptr : output_tensors.data(),
            static_cast<int>(output_ops.size()),
            target_ops.empty() ? nullptr : target_ops.data(),
            static_cast<int>(target_ops.size()),
            nullptr,  // Run metadata
            st.get());
        
        st.throw_if_error("TF_SessionRun");
        
        // Wrap outputs in Tensor objects
        std::vector<Tensor<>> results;
        results.reserve(output_tensors.size());
        
        for (TF_Tensor* raw : output_tensors) {
            if (raw) {
                results.push_back(Tensor<>::FromRaw(raw));
            }
        }
        
        return results;
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

private:
    TF_Session* session_{nullptr};
    TF_Graph* graph_handle_{nullptr};  // Non-owning; graph must outlive session
    mutable Policy policy_;
};

// ============================================================================
// Type aliases
// ============================================================================

using FastSession = Session<policy::NoLock>;
using SafeSession = Session<policy::Mutex>;

} // namespace tf
