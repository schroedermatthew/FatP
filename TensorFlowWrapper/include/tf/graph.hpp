// tf/graph.hpp
// RAII wrapper for TF_Graph with thread-safe operations
//
// MERGED IMPLEMENTATION - Best of ChatGPT + Claude:
// - ChatGPT: SetAttrTensor signature fix, debug assertion
// - Claude: Comprehensive attribute setters, import options
//
// Fixes applied:
// - P0: Guard lifetime fixed in GetOperation
// - P1: OperationBuilder holds lock for its ENTIRE lifetime
// - P2: OperationBuilder now holds lock until Finish()

#pragma once

#include <cassert>
#include <cstdio>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

extern "C" {
#include <tensorflow/c/c_api.h>
}

#include "tf/format.hpp"
#include "tf/operation.hpp"
#include "tf/policy.hpp"
#include "tf/status.hpp"
#include "tf/tensor.hpp"

namespace tf {

// Forward declaration
template<policy::LockPolicy Policy>
class Graph;

// ============================================================================
// OperationBuilder - Fluent builder for graph operations
// ============================================================================
// CRITICAL: This builder holds an EXCLUSIVE LOCK on the graph for its
// ENTIRE lifetime. The lock is released when Finish() is called (consuming
// the builder) or when the builder is destroyed without finishing.

template<policy::LockPolicy Policy>
class OperationBuilder {
public:
    using guard_type = decltype(std::declval<const Policy&>().scoped_lock());
    
    /// Construct builder (called by Graph::NewOperation)
    OperationBuilder(TF_Graph* graph,
                     const std::string& op_type,
                     const std::string& name,
                     guard_type guard)
        : graph_(graph)
        , guard_(std::move(guard))
        , desc_(TF_NewOperation(graph, op_type.c_str(), name.c_str()))
        , finished_(false)
    {
        if (!desc_) {
            throw std::runtime_error(tf::detail::format(
                "TF_NewOperation failed: type='{}', name='{}'", op_type, name));
        }
    }
    
    /// Destructor - cleans up if not finished, logs warning in debug builds
    ~OperationBuilder() noexcept {
        if (!finished_ && desc_) {
            // Operation was never finished - clean up the description
            // This can happen during exception unwinding, so we must not throw/assert
            TF_DeleteOperationDescription(desc_);
            
            // In debug builds, log to stderr (safe during unwinding)
            #ifndef NDEBUG
            std::fprintf(stderr, 
                "[tf_wrapper WARNING] OperationBuilder destroyed without Finish() - "
                "operation was discarded\n");
            #endif
        }
    }
    
    // Non-copyable
    OperationBuilder(const OperationBuilder&) = delete;
    OperationBuilder& operator=(const OperationBuilder&) = delete;
    
    // Movable (transfers lock and description ownership)
    OperationBuilder(OperationBuilder&& other) noexcept
        : graph_(other.graph_)
        , guard_(std::move(other.guard_))
        , desc_(other.desc_)
        , finished_(other.finished_)
    {
        other.desc_ = nullptr;
        other.finished_ = true;  // Prevent assertion in moved-from destructor
    }
    
    OperationBuilder& operator=(OperationBuilder&& other) noexcept {
        if (this != &other) {
            graph_ = other.graph_;
            guard_ = std::move(other.guard_);
            desc_ = other.desc_;
            finished_ = other.finished_;
            other.desc_ = nullptr;
            other.finished_ = true;
        }
        return *this;
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Attribute setters (fluent interface, all return *this)
    // Both lvalue (&) and rvalue (&&) overloads for flexible chaining
    // ─────────────────────────────────────────────────────────────────
    
    /// Set tensor attribute (e.g., for Const operations)
    OperationBuilder& SetAttrTensor(const char* name, TF_Tensor* tensor) & {
        Status st;
        TF_SetAttrTensor(desc_, name, tensor, st.get());
        st.throw_if_error(tf::detail::format("SetAttrTensor('{}')", name));
        return *this;
    }
    OperationBuilder&& SetAttrTensor(const char* name, TF_Tensor* tensor) && {
        return std::move(SetAttrTensor(name, tensor));
    }
    
    /// Set data type attribute
    OperationBuilder& SetAttrType(const char* name, TF_DataType dtype) & {
        TF_SetAttrType(desc_, name, dtype);
        return *this;
    }
    OperationBuilder&& SetAttrType(const char* name, TF_DataType dtype) && {
        return std::move(SetAttrType(name, dtype));
    }
    
    /// Set multiple data types
    OperationBuilder& SetAttrTypeList(const char* name, 
                                       std::span<const TF_DataType> types) & {
        TF_SetAttrTypeList(desc_, name, types.data(), static_cast<int>(types.size()));
        return *this;
    }
    OperationBuilder&& SetAttrTypeList(const char* name, 
                                        std::span<const TF_DataType> types) && {
        return std::move(SetAttrTypeList(name, types));
    }
    
    /// Set shape attribute
    OperationBuilder& SetAttrShape(const char* name,
                                    std::span<const std::int64_t> dims) & {
        TF_SetAttrShape(desc_, name, dims.data(), static_cast<int>(dims.size()));
        return *this;
    }
    OperationBuilder&& SetAttrShape(const char* name,
                                     std::span<const std::int64_t> dims) && {
        return std::move(SetAttrShape(name, dims));
    }
    
    /// Set integer attribute
    OperationBuilder& SetAttrInt(const char* name, std::int64_t value) & {
        TF_SetAttrInt(desc_, name, value);
        return *this;
    }
    OperationBuilder&& SetAttrInt(const char* name, std::int64_t value) && {
        return std::move(SetAttrInt(name, value));
    }
    
    /// Set multiple integers
    OperationBuilder& SetAttrIntList(const char* name,
                                      std::span<const std::int64_t> values) & {
        TF_SetAttrIntList(desc_, name, values.data(), static_cast<int>(values.size()));
        return *this;
    }
    OperationBuilder&& SetAttrIntList(const char* name,
                                       std::span<const std::int64_t> values) && {
        return std::move(SetAttrIntList(name, values));
    }
    
    /// Set float attribute
    OperationBuilder& SetAttrFloat(const char* name, float value) & {
        TF_SetAttrFloat(desc_, name, value);
        return *this;
    }
    OperationBuilder&& SetAttrFloat(const char* name, float value) && {
        return std::move(SetAttrFloat(name, value));
    }
    
    /// Set multiple floats
    OperationBuilder& SetAttrFloatList(const char* name,
                                        std::span<const float> values) & {
        TF_SetAttrFloatList(desc_, name, values.data(), static_cast<int>(values.size()));
        return *this;
    }
    OperationBuilder&& SetAttrFloatList(const char* name,
                                         std::span<const float> values) && {
        return std::move(SetAttrFloatList(name, values));
    }
    
    /// Set boolean attribute
    OperationBuilder& SetAttrBool(const char* name, bool value) & {
        TF_SetAttrBool(desc_, name, value ? 1 : 0);
        return *this;
    }
    OperationBuilder&& SetAttrBool(const char* name, bool value) && {
        return std::move(SetAttrBool(name, value));
    }
    
    /// Set string attribute
    OperationBuilder& SetAttrString(const char* name, std::string_view value) & {
        TF_SetAttrString(desc_, name, value.data(), value.size());
        return *this;
    }
    OperationBuilder&& SetAttrString(const char* name, std::string_view value) && {
        return std::move(SetAttrString(name, value));
    }
    
    /// Set function attribute
    OperationBuilder& SetAttrFuncName(const char* name, std::string_view func_name) & {
        TF_SetAttrFuncName(desc_, name, func_name.data(), func_name.size());
        return *this;
    }
    OperationBuilder&& SetAttrFuncName(const char* name, std::string_view func_name) && {
        return std::move(SetAttrFuncName(name, func_name));
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Input connections
    // ─────────────────────────────────────────────────────────────────
    
    /// Add single input
    OperationBuilder& AddInput(TF_Output input) & {
        TF_AddInput(desc_, input);
        return *this;
    }
    OperationBuilder&& AddInput(TF_Output input) && {
        return std::move(AddInput(input));
    }
    
    /// Add input from Operation handle
    OperationBuilder& AddInput(const Operation& op, int index = 0) & {
        TF_AddInput(desc_, op.output(index));
        return *this;
    }
    OperationBuilder&& AddInput(const Operation& op, int index = 0) && {
        return std::move(AddInput(op, index));
    }
    
    /// Add input from raw TF_Operation*
    OperationBuilder& AddInput(TF_Operation* op, int index = 0) & {
        TF_AddInput(desc_, TF_Output{op, index});
        return *this;
    }
    OperationBuilder&& AddInput(TF_Operation* op, int index = 0) && {
        return std::move(AddInput(op, index));
    }
    
    /// Add multiple inputs
    OperationBuilder& AddInputList(std::span<const TF_Output> inputs) & {
        TF_AddInputList(desc_, inputs.data(), static_cast<int>(inputs.size()));
        return *this;
    }
    OperationBuilder&& AddInputList(std::span<const TF_Output> inputs) && {
        return std::move(AddInputList(inputs));
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Control dependencies
    // ─────────────────────────────────────────────────────────────────
    
    /// Add control dependency
    OperationBuilder& AddControlInput(TF_Operation* op) & {
        TF_AddControlInput(desc_, op);
        return *this;
    }
    OperationBuilder&& AddControlInput(TF_Operation* op) && {
        return std::move(AddControlInput(op));
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Device placement
    // ─────────────────────────────────────────────────────────────────
    
    /// Set device (e.g., "/device:GPU:0")
    OperationBuilder& SetDevice(const char* device) & {
        TF_SetDevice(desc_, device);
        return *this;
    }
    OperationBuilder&& SetDevice(const char* device) && {
        return std::move(SetDevice(device));
    }
    
    /// Colocate with another operation
    OperationBuilder& ColocateWith(TF_Operation* op) & {
        TF_ColocateWith(desc_, op);
        return *this;
    }
    OperationBuilder&& ColocateWith(TF_Operation* op) && {
        return std::move(ColocateWith(op));
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Finish - completes the operation and releases the lock
    // NOTE: This consumes the builder (r-value ref qualifier)
    // ─────────────────────────────────────────────────────────────────
    
    /// Finalize and return the created operation
    [[nodiscard]] TF_Operation* Finish() && {
        Status st;
        TF_Operation* op = TF_FinishOperation(desc_, st.get());
        desc_ = nullptr;
        finished_ = true;
        st.throw_if_error("TF_FinishOperation");
        return op;
    }

private:
    TF_Graph* graph_;
    guard_type guard_;  // Holds lock for builder's entire lifetime
    TF_OperationDescription* desc_;
    bool finished_;
};

// ============================================================================
// Graph - RAII wrapper for TF_Graph
// ============================================================================

template<policy::LockPolicy Policy = policy::NoLock>
class Graph {
public:
    using policy_type = Policy;
    
    /// Create an empty graph
    Graph() : graph_(TF_NewGraph()) {
        if (!graph_) {
            throw std::runtime_error("TF_NewGraph failed");
        }
    }
    
    ~Graph() noexcept {
        if (graph_) TF_DeleteGraph(graph_);
    }
    
    // Non-copyable
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;
    
    // Movable
    Graph(Graph&& other) noexcept
        : graph_(other.graph_)
        , policy_(std::move(other.policy_))
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
    
    // ─────────────────────────────────────────────────────────────────
    // Import GraphDef
    // ─────────────────────────────────────────────────────────────────
    
    /// Import a serialized GraphDef protobuf
    void ImportGraphDef(const void* proto, std::size_t proto_len,
                        const char* prefix = "") {
        [[maybe_unused]] auto guard = policy_.scoped_lock();  // Exclusive for mutation
        
        TF_Buffer* buf = TF_NewBufferFromString(proto, proto_len);
        TF_ImportGraphDefOptions* opts = TF_NewImportGraphDefOptions();
        
        if (prefix && prefix[0] != '\0') {
            TF_ImportGraphDefOptionsSetPrefix(opts, prefix);
        }
        
        Status st;
        TF_GraphImportGraphDef(graph_, buf, opts, st.get());
        
        TF_DeleteImportGraphDefOptions(opts);
        TF_DeleteBuffer(buf);
        
        st.throw_if_error("TF_GraphImportGraphDef");
    }
    
    /// Import from a TF_Buffer
    void ImportGraphDef(const TF_Buffer* buf, const char* prefix = "") {
        if (!buf || !buf->data) {
            throw std::invalid_argument("ImportGraphDef: null buffer");
        }
        ImportGraphDef(buf->data, buf->length, prefix);
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Operation lookup
    // ─────────────────────────────────────────────────────────────────
    
    /// Find operation by name (returns nullopt if not found)
    [[nodiscard]] std::optional<TF_Operation*> GetOperation(
        const std::string& name) const 
    {
        [[maybe_unused]] auto guard = policy_.scoped_shared();  // Shared for read
        TF_Operation* op = TF_GraphOperationByName(graph_, name.c_str());
        return op ? std::optional{op} : std::nullopt;
    }  // Lock released here
    
    /// Find operation by name (throws if not found)
    [[nodiscard]] TF_Operation* GetOperationOrThrow(const std::string& name) const {
        auto opt = GetOperation(name);
        if (!opt) {
            throw std::runtime_error(tf::detail::format(
                "Operation '{}' not found in graph", name));
        }
        return *opt;
    }
    
    /// Check if operation exists
    [[nodiscard]] bool HasOperation(const std::string& name) const {
        return GetOperation(name).has_value();
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Create new operations
    // ─────────────────────────────────────────────────────────────────
    
    /// Create a new operation builder (holds lock until Finish())
    [[nodiscard]] OperationBuilder<Policy> NewOperation(
        const std::string& op_type,
        const std::string& name)
    {
        auto guard = policy_.scoped_lock();  // Lock acquired
        return OperationBuilder<Policy>(graph_, op_type, name, std::move(guard));
    }  // Lock transferred to builder
    
    // ─────────────────────────────────────────────────────────────────
    // Graph info
    // ─────────────────────────────────────────────────────────────────
    
    /// Get all operations in the graph
    [[nodiscard]] std::vector<TF_Operation*> GetAllOperations() const {
        [[maybe_unused]] auto guard = policy_.scoped_shared();
        
        std::vector<TF_Operation*> ops;
        std::size_t pos = 0;
        TF_Operation* op;
        
        while ((op = TF_GraphNextOperation(graph_, &pos)) != nullptr) {
            ops.push_back(op);
        }
        
        return ops;
    }
    
    /// Get number of operations (efficient - no allocation)
    [[nodiscard]] std::size_t num_operations() const {
        [[maybe_unused]] auto guard = policy_.scoped_shared();
        
        std::size_t count = 0;
        std::size_t pos = 0;
        while (TF_GraphNextOperation(graph_, &pos) != nullptr) {
            ++count;
        }
        return count;
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Graph inspection (debugging)
    // ─────────────────────────────────────────────────────────────────
    
    /// Serialize graph to GraphDef protobuf
    /// The returned bytes can be:
    /// - Written to a .pb file for visualization in TensorBoard
    /// - Loaded by another Graph using ImportGraphDef
    /// - Parsed using protobuf to inspect graph structure
    [[nodiscard]] std::vector<std::uint8_t> ToGraphDef() const {
        [[maybe_unused]] auto guard = policy_.scoped_shared();
        
        TF_Buffer* buf = TF_NewBuffer();
        if (!buf) {
            throw std::runtime_error("TF_NewBuffer failed");
        }
        
        Status st;
        TF_GraphToGraphDef(graph_, buf, st.get());
        
        if (!st.ok()) {
            TF_DeleteBuffer(buf);
            st.throw_if_error("TF_GraphToGraphDef");
        }
        
        std::vector<std::uint8_t> result;
        if (buf->data && buf->length > 0) {
            const auto* p = static_cast<const std::uint8_t*>(buf->data);
            result.assign(p, p + buf->length);
        }
        
        TF_DeleteBuffer(buf);
        return result;
    }
    
    /// Information about an operation's input or output
    struct TensorPort {
        std::string op_name;      ///< Name of the operation
        std::string op_type;      ///< Type of the operation (e.g., "Placeholder", "Const")
        int index;                ///< Port index
        TF_DataType dtype;        ///< Data type
        std::string full_name;    ///< Full tensor name (op_name:index)
    };
    
    /// Get information about all placeholder operations (typical feed points)
    /// Placeholders are the usual entry points for feeding data into a graph
    [[nodiscard]] std::vector<TensorPort> GetPlaceholders() const {
        [[maybe_unused]] auto guard = policy_.scoped_shared();
        
        std::vector<TensorPort> placeholders;
        std::size_t pos = 0;
        TF_Operation* op;
        
        while ((op = TF_GraphNextOperation(graph_, &pos)) != nullptr) {
            const char* op_type = TF_OperationOpType(op);
            if (std::string_view(op_type) == "Placeholder" ||
                std::string_view(op_type) == "PlaceholderV2") {
                
                const char* name = TF_OperationName(op);
                const int num_outputs = TF_OperationNumOutputs(op);
                
                for (int i = 0; i < num_outputs; ++i) {
                    TensorPort port;
                    port.op_name = name;
                    port.op_type = op_type;
                    port.index = i;
                    port.dtype = TF_OperationOutputType(TF_Output{op, i});
                    port.full_name = std::string(name) + ":" + std::to_string(i);
                    placeholders.push_back(std::move(port));
                }
            }
        }
        
        return placeholders;
    }
    
    /// Get information about operations that have no consumers (typical fetch points)
    /// These are operations whose outputs are not used by any other operation
    [[nodiscard]] std::vector<TensorPort> GetOutputs() const {
        [[maybe_unused]] auto guard = policy_.scoped_shared();
        
        std::vector<TensorPort> outputs;
        std::size_t pos = 0;
        TF_Operation* op;
        
        while ((op = TF_GraphNextOperation(graph_, &pos)) != nullptr) {
            const char* name = TF_OperationName(op);
            const char* op_type = TF_OperationOpType(op);
            const int num_outputs = TF_OperationNumOutputs(op);
            
            for (int i = 0; i < num_outputs; ++i) {
                TF_Output output{op, i};
                
                // Check if this output has any consumers
                const int num_consumers = TF_OperationOutputNumConsumers(output);
                
                if (num_consumers == 0) {
                    TensorPort port;
                    port.op_name = name;
                    port.op_type = op_type;
                    port.index = i;
                    port.dtype = TF_OperationOutputType(output);
                    port.full_name = std::string(name) + ":" + std::to_string(i);
                    outputs.push_back(std::move(port));
                }
            }
        }
        
        return outputs;
    }
    
    /// Get all operations of a specific type
    [[nodiscard]] std::vector<TF_Operation*> GetOperationsByType(
        std::string_view op_type) const
    {
        [[maybe_unused]] auto guard = policy_.scoped_shared();
        
        std::vector<TF_Operation*> ops;
        std::size_t pos = 0;
        TF_Operation* op;
        
        while ((op = TF_GraphNextOperation(graph_, &pos)) != nullptr) {
            if (std::string_view(TF_OperationOpType(op)) == op_type) {
                ops.push_back(op);
            }
        }
        
        return ops;
    }
    
    /// Print graph summary to a string (for debugging)
    [[nodiscard]] std::string DebugString() const {
        [[maybe_unused]] auto guard = policy_.scoped_shared();
        
        std::string result;
        result += "Graph with " + std::to_string(num_operations()) + " operations:\n";
        
        std::size_t pos = 0;
        TF_Operation* op;
        
        while ((op = TF_GraphNextOperation(graph_, &pos)) != nullptr) {
            const char* name = TF_OperationName(op);
            const char* type = TF_OperationOpType(op);
            const int num_inputs = TF_OperationNumInputs(op);
            const int num_outputs = TF_OperationNumOutputs(op);
            
            result += "  ";
            result += name;
            result += " (";
            result += type;
            result += ") inputs=";
            result += std::to_string(num_inputs);
            result += " outputs=";
            result += std::to_string(num_outputs);
            result += "\n";
        }
        
        return result;
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Raw handle
    // ─────────────────────────────────────────────────────────────────
    
    [[nodiscard]] TF_Graph* handle() const noexcept { return graph_; }

private:
    TF_Graph* graph_{nullptr};
    mutable Policy policy_;
};

// ============================================================================
// Type aliases
// ============================================================================

using FastGraph = Graph<policy::NoLock>;
using SafeGraph = Graph<policy::Mutex>;
using SharedGraph = Graph<policy::SharedMutex>;

} // namespace tf
