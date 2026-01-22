// tf/operation.hpp
// Non-owning handle to a TF_Operation within a Graph
//
// Fixes applied:
// - P0: Missing <stdexcept> include
// - P1: No noexcept on string-returning functions

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

extern "C" {
#include <tensorflow/c/c_api.h>
}

#include "tf/status.hpp"

namespace tf {

// ============================================================================
// Operation - Non-owning wrapper for TF_Operation*
// ============================================================================
// The graph owns the operation; this is just a convenient handle.

class Operation {
public:
    /// Construct from raw handle (non-null)
    explicit Operation(TF_Operation* op) : op_(op) {
        if (!op_) {
            throw std::invalid_argument("Operation: null TF_Operation*");
        }
    }
    
    // Default copy/move (it's just a pointer handle)
    Operation(const Operation&) = default;
    Operation& operator=(const Operation&) = default;
    Operation(Operation&&) = default;
    Operation& operator=(Operation&&) = default;
    
    // ─────────────────────────────────────────────────────────────────
    // Handle access
    // ─────────────────────────────────────────────────────────────────
    
    [[nodiscard]] TF_Operation* handle() const noexcept { return op_; }
    
    // ─────────────────────────────────────────────────────────────────
    // Metadata (NOTE: not noexcept - can allocate)
    // ─────────────────────────────────────────────────────────────────
    
    /// Get operation name
    [[nodiscard]] std::string name() const { 
        return TF_OperationName(op_); 
    }
    
    /// Get operation type (e.g., "Const", "MatMul", "Add")
    [[nodiscard]] std::string op_type() const { 
        return TF_OperationOpType(op_); 
    }
    
    /// Get device placement
    [[nodiscard]] std::string device() const { 
        return TF_OperationDevice(op_); 
    }
    
    // ─────────────────────────────────────────────────────────────────
    // Input/Output topology
    // ─────────────────────────────────────────────────────────────────
    
    [[nodiscard]] int num_inputs() const noexcept { 
        return TF_OperationNumInputs(op_); 
    }
    
    [[nodiscard]] int num_outputs() const noexcept { 
        return TF_OperationNumOutputs(op_); 
    }
    
    /// Get output at index (default 0)
    [[nodiscard]] TF_Output output(int index = 0) const noexcept { 
        return TF_Output{op_, index}; 
    }
    
    /// Get input at index (default 0)
    [[nodiscard]] TF_Input input(int index = 0) const noexcept {
        return TF_Input{op_, index};
    }
    
    /// Get output data type
    [[nodiscard]] TF_DataType output_type(int index = 0) const noexcept {
        return TF_OperationOutputType(TF_Output{op_, index});
    }
    
    /// Get number of dimensions for output (-1 if unknown).
    ///
    /// TensorFlow requires the owning graph to query shape info.
    [[nodiscard]] int output_num_dims(TF_Graph* graph, int index = 0) const {
        if (!graph) {
            throw std::invalid_argument("Operation::output_num_dims: null TF_Graph*");
        }

        Status st;
        const int ndims = TF_GraphGetTensorNumDims(graph, TF_Output{op_, index}, st.get());
        st.throw_if_error("TF_GraphGetTensorNumDims");
        return ndims;
    }

private:
    TF_Operation* op_;  // Non-owning
};

// ============================================================================
// Output Helper - Convenience functions for creating TF_Output
// ============================================================================

/// Create TF_Output from raw operation pointer
[[nodiscard]] inline TF_Output Output(TF_Operation* op, int index = 0) noexcept {
    return TF_Output{op, index};
}

/// Create TF_Output from Operation wrapper
[[nodiscard]] inline TF_Output Output(const Operation& op, int index = 0) noexcept {
    return op.output(index);
}

} // namespace tf
