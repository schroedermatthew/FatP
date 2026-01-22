#include <tensorflow/c/c_api.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>

// ============================================================================
// Extremely small, behavior-lite stub of the TF C API.
//
// Goals:
// - Allow this wrapper to compile + run unit tests without TensorFlow.
// - Provide predictable, safe behavior for status/tensor/graph/session calls.
//
// Non-goals:
// - Any numerical correctness.
// - ABI/API compatibility with real TensorFlow.
// ============================================================================

struct TF_Status {
    TF_Code code{TF_OK};
    std::string message;
};

struct TF_Tensor {
    TF_DataType dtype{TF_FLOAT};
    std::vector<int64_t> dims;
    std::vector<std::byte> storage;
    void* external_data{nullptr};
    size_t external_len{0};
    TF_TensorDeallocator deallocator{nullptr};
    void* deallocator_arg{nullptr};

    bool owns_storage{true};
};

struct TF_Graph {
    std::vector<std::unique_ptr<TF_Operation>> ops;
};

struct TF_Operation {
    std::string op_type;
    std::string name;
    std::string device;

    std::vector<TF_Output> inputs;
    int num_outputs{1};
    TF_DataType output_type{TF_FLOAT};

    // Minimal attribute storage needed by the wrapper/tests.
    // Real TF supports many attr types; we only store tensor attrs.
    std::unordered_map<std::string, std::unique_ptr<TF_Tensor>> tensor_attrs;
};

struct TF_OperationDescription {
    TF_Graph* graph{nullptr};
    std::unique_ptr<TF_Operation> op;

    explicit TF_OperationDescription(TF_Graph* g) : graph(g), op(std::make_unique<TF_Operation>()) {}
};

struct TF_ImportGraphDefOptions {
    std::string prefix;
};

struct TF_SessionOptions {
    std::string target;
    std::vector<std::byte> config;
};

struct TF_Session {
    TF_Graph* graph{nullptr};
    bool closed{false};
};

// Stub device info
struct StubDevice {
    std::string name;
    std::string type;
    int64_t memory_bytes;
};

struct TF_DeviceList {
    std::vector<StubDevice> devices;
};

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

static void set_status(TF_Status* st, TF_Code code, const char* msg)
{
    if (!st)
    {
        return;
    }

    st->code = code;
    st->message = msg ? msg : "";
}

static size_t dtype_size(TF_DataType t)
{
    switch (t)
    {
        case TF_FLOAT:      return 4;
        case TF_DOUBLE:     return 8;
        case TF_INT32:      return 4;
        case TF_UINT8:      return 1;
        case TF_INT16:      return 2;
        case TF_INT8:       return 1;
        case TF_INT64:      return 8;
        case TF_BOOL:       return 1;
        case TF_UINT16:     return 2;
        case TF_UINT32:     return 4;
        case TF_UINT64:     return 8;
        case TF_COMPLEX64:  return 8;
        case TF_COMPLEX128: return 16;
        case TF_HALF:       return 2;
        case TF_BFLOAT16:   return 2;
        default:            return 0;
    }
}

static int64_t element_count(const std::vector<int64_t>& dims)
{
    if (dims.empty())
    {
        return 1;
    }

    int64_t prod = 1;
    for (const int64_t d : dims)
    {
        if (d < 0)
        {
            return 0;
        }
        if (d == 0)
        {
            return 0;
        }
        prod *= d;
    }
    return prod;
}

static std::unique_ptr<TF_Tensor> clone_tensor(const TF_Tensor* t)
{
    if (!t)
    {
        return nullptr;
    }

    auto out = std::make_unique<TF_Tensor>();
    out->dtype = t->dtype;
    out->dims = t->dims;

    const size_t bytes = t->owns_storage ? t->storage.size() : t->external_len;
    out->storage.resize(bytes);
    out->owns_storage = true;

    const void* src = t->owns_storage ? static_cast<const void*>(t->storage.data()) : t->external_data;
    if (bytes != 0 && src)
    {
        std::memcpy(out->storage.data(), src, bytes);
    }
    return out;
}

// ----------------------------------------------------------------------------
// Status
// ----------------------------------------------------------------------------

TF_Status* TF_NewStatus(void)
{
    return new TF_Status{};
}

void TF_DeleteStatus(TF_Status* st)
{
    delete st;
}

TF_Code TF_GetCode(const TF_Status* st)
{
    return st ? st->code : TF_UNKNOWN;
}

const char* TF_Message(const TF_Status* st)
{
    if (!st)
    {
        return "TF_Status(null)";
    }
    return st->message.c_str();
}

void TF_SetStatus(TF_Status* st, TF_Code code, const char* msg)
{
    set_status(st, code, msg);
}

// ----------------------------------------------------------------------------
// Buffer
// ----------------------------------------------------------------------------

TF_Buffer* TF_NewBuffer(void)
{
    auto* b = new TF_Buffer{};
    b->data = nullptr;
    b->length = 0;
    b->data_deallocator = nullptr;
    return b;
}

TF_Buffer* TF_NewBufferFromString(const void* proto, size_t proto_len)
{
    auto* b = new TF_Buffer{};
    if (!proto || proto_len == 0)
    {
        b->data = nullptr;
        b->length = 0;
        b->data_deallocator = nullptr;
        return b;
    }

    void* data = ::operator new(proto_len);
    std::memcpy(data, proto, proto_len);

    b->data = data;
    b->length = proto_len;
    b->data_deallocator = [](void* d, size_t) {
        ::operator delete(d);
    };

    return b;
}

void TF_DeleteBuffer(TF_Buffer* b)
{
    if (!b)
    {
        return;
    }

    if (b->data && b->data_deallocator)
    {
        b->data_deallocator(b->data, b->length);
    }

    delete b;
}

// ----------------------------------------------------------------------------
// Tensor
// ----------------------------------------------------------------------------

TF_Tensor* TF_AllocateTensor(TF_DataType dt, const int64_t* dims, int num_dims, size_t len)
{
    auto* t = new TF_Tensor{};
    t->dtype = dt;
    t->dims.assign(dims ? dims : nullptr, dims ? dims + num_dims : nullptr);
    t->storage.resize(len);
    t->owns_storage = true;
    return t;
}

TF_Tensor* TF_NewTensor(
    TF_DataType dt,
    const int64_t* dims,
    int num_dims,
    void* data,
    size_t len,
    TF_TensorDeallocator deallocator,
    void* deallocator_arg)
{
    auto* t = new TF_Tensor{};
    t->dtype = dt;
    t->dims.assign(dims ? dims : nullptr, dims ? dims + num_dims : nullptr);

    t->external_data = data;
    t->external_len = len;
    t->deallocator = deallocator;
    t->deallocator_arg = deallocator_arg;
    t->owns_storage = false;
    return t;
}

void TF_DeleteTensor(TF_Tensor* t)
{
    if (!t)
    {
        return;
    }

    if (!t->owns_storage && t->deallocator && t->external_data)
    {
        t->deallocator(t->external_data, t->external_len, t->deallocator_arg);
    }

    delete t;
}

TF_DataType TF_TensorType(const TF_Tensor* t)
{
    return t ? t->dtype : TF_FLOAT;
}

void* TF_TensorData(const TF_Tensor* t)
{
    if (!t)
    {
        return nullptr;
    }

    if (t->owns_storage)
    {
        return const_cast<std::byte*>(t->storage.data());
    }

    return t->external_data;
}

size_t TF_TensorByteSize(const TF_Tensor* t)
{
    if (!t)
    {
        return 0;
    }

    if (t->owns_storage)
    {
        return t->storage.size();
    }

    return t->external_len;
}

int TF_NumDims(const TF_Tensor* t)
{
    return t ? static_cast<int>(t->dims.size()) : 0;
}

int64_t TF_Dim(const TF_Tensor* t, int dim_index)
{
    if (!t)
    {
        return 0;
    }

    if (dim_index < 0 || static_cast<size_t>(dim_index) >= t->dims.size())
    {
        return 0;
    }

    return t->dims[static_cast<size_t>(dim_index)];
}

int64_t TF_TensorElementCount(const TF_Tensor* t)
{
    if (!t)
    {
        return 0;
    }

    return element_count(t->dims);
}

size_t TF_DataTypeSize(TF_DataType dt)
{
    return dtype_size(dt);
}

// ----------------------------------------------------------------------------
// Graph / operations
// ----------------------------------------------------------------------------

TF_Graph* TF_NewGraph(void)
{
    return new TF_Graph{};
}

void TF_DeleteGraph(TF_Graph* g)
{
    delete g;
}

TF_OperationDescription* TF_NewOperation(TF_Graph* g, const char* op_type, const char* oper_name)
{
    if (!g)
    {
        return nullptr;
    }

    auto* d = new TF_OperationDescription(g);
    d->op->op_type = op_type ? op_type : "";
    d->op->name = oper_name ? oper_name : "";
    return d;
}

void TF_SetDevice(TF_OperationDescription* desc, const char* device)
{
    if (desc && desc->op)
    {
        desc->op->device = device ? device : "";
    }
}

void TF_AddInput(TF_OperationDescription* desc, TF_Output input)
{
    if (desc && desc->op)
    {
        desc->op->inputs.push_back(input);
    }
}

void TF_AddInputList(TF_OperationDescription* desc, const TF_Output* inputs, int num_inputs)
{
    if (!desc || !desc->op || !inputs || num_inputs <= 0)
    {
        return;
    }

    desc->op->inputs.insert(desc->op->inputs.end(), inputs, inputs + num_inputs);
}

void TF_AddControlInput(TF_OperationDescription*, TF_Operation*)
{
}

void TF_ColocateWith(TF_OperationDescription*, TF_Operation*)
{
}

void TF_SetAttrBool(TF_OperationDescription*, const char*, unsigned char)
{
}

void TF_SetAttrInt(TF_OperationDescription*, const char*, int64_t)
{
}

void TF_SetAttrFloat(TF_OperationDescription*, const char*, float)
{
}

void TF_SetAttrType(TF_OperationDescription*, const char*, TF_DataType)
{
}

void TF_SetAttrShape(TF_OperationDescription*, const char*, const int64_t*, int)
{
}

void TF_SetAttrString(TF_OperationDescription*, const char*, const void*, size_t)
{
}

void TF_SetAttrFuncName(TF_OperationDescription*, const char*, const char*, size_t)
{
}

void TF_SetAttrIntList(TF_OperationDescription*, const char*, const int64_t*, int)
{
}

void TF_SetAttrFloatList(TF_OperationDescription*, const char*, const float*, int)
{
}

void TF_SetAttrTypeList(TF_OperationDescription*, const char*, const TF_DataType*, int)
{
}

void TF_SetAttrTensor(TF_OperationDescription* desc, const char* attr_name, TF_Tensor* value, TF_Status* status)
{
    if (!desc || !desc->op)
    {
        set_status(status, TF_INVALID_ARGUMENT, "TF_SetAttrTensor: invalid description");
        return;
    }

    if (!attr_name || attr_name[0] == '\0')
    {
        set_status(status, TF_INVALID_ARGUMENT, "TF_SetAttrTensor: empty attr name");
        return;
    }

    // OperationDescription must own/copy attributes; do not alias caller memory.
    desc->op->tensor_attrs[std::string(attr_name)] = clone_tensor(value);
    set_status(status, TF_OK, "");
}

TF_Operation* TF_FinishOperation(TF_OperationDescription* desc, TF_Status* status)
{
    if (!desc || !desc->graph || !desc->op)
    {
        set_status(status, TF_INVALID_ARGUMENT, "TF_FinishOperation: invalid description");
        return nullptr;
    }

    TF_Operation* raw = desc->op.get();
    desc->graph->ops.push_back(std::move(desc->op));
    set_status(status, TF_OK, "");
    delete desc;
    return raw;
}

void TF_DeleteOperationDescription(TF_OperationDescription* desc)
{
    delete desc;
}

TF_Operation* TF_GraphOperationByName(TF_Graph* g, const char* oper_name)
{
    if (!g || !oper_name)
    {
        return nullptr;
    }

    for (auto& op : g->ops)
    {
        if (op && op->name == oper_name)
        {
            return op.get();
        }
    }

    return nullptr;
}

TF_Operation* TF_GraphNextOperation(TF_Graph* g, size_t* pos)
{
    if (!g || !pos)
    {
        return nullptr;
    }

    if (*pos >= g->ops.size())
    {
        return nullptr;
    }

    TF_Operation* out = g->ops[*pos].get();
    ++(*pos);
    return out;
}

int TF_GraphGetTensorNumDims(TF_Graph*, TF_Output, TF_Status* status)
{
    set_status(status, TF_UNIMPLEMENTED, "TF_GraphGetTensorNumDims: stub");
    return -1;
}

TF_DataType TF_OperationOutputType(TF_Output out)
{
    if (!out.oper)
    {
        return TF_FLOAT;
    }

    return out.oper->output_type;
}

const char* TF_OperationName(TF_Operation* op)
{
    return op ? op->name.c_str() : "";
}

const char* TF_OperationOpType(TF_Operation* op)
{
    return op ? op->op_type.c_str() : "";
}

const char* TF_OperationDevice(TF_Operation* op)
{
    return op ? op->device.c_str() : "";
}

int TF_OperationNumInputs(TF_Operation* op)
{
    return op ? static_cast<int>(op->inputs.size()) : 0;
}

int TF_OperationNumOutputs(TF_Operation* op)
{
    return op ? op->num_outputs : 0;
}

int TF_OperationOutputNumConsumers(TF_Output output)
{
    // In the stub, we need to search the graph to find consumers
    // Since we don't have direct access to the graph from TF_Output,
    // we'll just return 0 for outputs with no operation, or check if
    // any other op in the same graph uses this output as input.
    // 
    // For simplicity, this stub returns 0 for terminal ops (Const, Add, etc.)
    // and 1 for intermediate ops. Real TF does proper graph traversal.
    if (!output.oper)
    {
        return 0;
    }
    
    // In our stub, we don't have easy access to the graph from an operation.
    // Return 0 to indicate "no known consumers" - this is safe for GetOutputs()
    // which looks for ops with zero consumers.
    return 0;
}

void TF_GraphToGraphDef(TF_Graph* g, TF_Buffer* output_graph_def, TF_Status* status)
{
    if (!g)
    {
        set_status(status, TF_INVALID_ARGUMENT, "TF_GraphToGraphDef: null graph");
        return;
    }
    
    if (!output_graph_def)
    {
        set_status(status, TF_INVALID_ARGUMENT, "TF_GraphToGraphDef: null buffer");
        return;
    }
    
    // In the stub, we create a simple placeholder "GraphDef" that contains
    // basic info about the graph. Real TensorFlow would serialize to protobuf.
    // We'll create a simple text representation for debugging purposes.
    std::string graph_info = "STUB_GRAPH_DEF\n";
    graph_info += "num_operations: " + std::to_string(g->ops.size()) + "\n";
    
    for (const auto& op : g->ops)
    {
        graph_info += "op: " + op->name + " type: " + op->op_type + "\n";
    }
    
    // Allocate and copy to buffer
    size_t len = graph_info.size();
    void* data = ::operator new(len);
    std::memcpy(data, graph_info.data(), len);
    
    output_graph_def->data = data;
    output_graph_def->length = len;
    output_graph_def->data_deallocator = [](void* d, size_t) {
        ::operator delete(d);
    };
    
    set_status(status, TF_OK, "");
}

// ----------------------------------------------------------------------------
// Import graph
// ----------------------------------------------------------------------------

TF_ImportGraphDefOptions* TF_NewImportGraphDefOptions(void)
{
    return new TF_ImportGraphDefOptions{};
}

void TF_DeleteImportGraphDefOptions(TF_ImportGraphDefOptions* o)
{
    delete o;
}

void TF_ImportGraphDefOptionsSetPrefix(TF_ImportGraphDefOptions* o, const char* prefix)
{
    if (o)
    {
        o->prefix = prefix ? prefix : "";
    }
}

void TF_GraphImportGraphDef(TF_Graph*, const TF_Buffer*, const TF_ImportGraphDefOptions*, TF_Status* status)
{
    set_status(status, TF_UNIMPLEMENTED, "TF_GraphImportGraphDef: stub");
}

// ----------------------------------------------------------------------------
// Session
// ----------------------------------------------------------------------------

TF_SessionOptions* TF_NewSessionOptions(void)
{
    return new TF_SessionOptions{};
}

void TF_DeleteSessionOptions(TF_SessionOptions* o)
{
    delete o;
}

void TF_SetTarget(TF_SessionOptions* o, const char* target)
{
    if (o)
    {
        o->target = target ? target : "";
    }
}

void TF_SetConfig(TF_SessionOptions* o, const void* proto, size_t proto_len, TF_Status* status)
{
    if (!o)
    {
        set_status(status, TF_INVALID_ARGUMENT, "TF_SetConfig: null options");
        return;
    }

    o->config.assign(
        reinterpret_cast<const std::byte*>(proto),
        reinterpret_cast<const std::byte*>(proto) + proto_len);

    set_status(status, TF_OK, "");
}

TF_Session* TF_NewSession(TF_Graph* g, const TF_SessionOptions*, TF_Status* status)
{
    if (!g)
    {
        set_status(status, TF_INVALID_ARGUMENT, "TF_NewSession: null graph");
        return nullptr;
    }

    auto* s = new TF_Session{};
    s->graph = g;
    s->closed = false;
    set_status(status, TF_OK, "");
    return s;
}

void TF_CloseSession(TF_Session* s, TF_Status* status)
{
    if (!s)
    {
        set_status(status, TF_INVALID_ARGUMENT, "TF_CloseSession: null session");
        return;
    }

    s->closed = true;
    set_status(status, TF_OK, "");
}

void TF_DeleteSession(TF_Session* s, TF_Status* status)
{
    if (!s)
    {
        set_status(status, TF_INVALID_ARGUMENT, "TF_DeleteSession: null session");
        return;
    }

    delete s;
    set_status(status, TF_OK, "");
}

void TF_SessionRun(
    TF_Session* session,
    const TF_Buffer*,
    const TF_Output*,
    TF_Tensor* const*,
    int,
    const TF_Output* output_ops,
    TF_Tensor** output_values,
    int noutputs,
    const TF_Operation* const*,
    int,
    TF_Buffer*,
    TF_Status* status)
{
    if (!session)
    {
        set_status(status, TF_INVALID_ARGUMENT, "TF_SessionRun: null session");
        return;
    }

    if (session->closed)
    {
        set_status(status, TF_FAILED_PRECONDITION, "TF_SessionRun: session closed");
        return;
    }

    auto eval_op = [&](auto&& self, TF_Operation* op,
                       std::unordered_map<TF_Operation*, std::unique_ptr<TF_Tensor>>& cache)
        -> TF_Tensor*
    {
        if (!op)
        {
            return nullptr;
        }

        if (auto it = cache.find(op); it != cache.end())
        {
            return it->second.get();
        }

        if (op->op_type == "Const")
        {
            auto it = op->tensor_attrs.find("value");
            if (it == op->tensor_attrs.end() || !it->second)
            {
                return nullptr;
            }
            cache.emplace(op, clone_tensor(it->second.get()));
            return cache[op].get();
        }

        // Identity just passes through
        if (op->op_type == "Identity")
        {
            if (op->inputs.empty())
            {
                return nullptr;
            }
            TF_Tensor* input = self(self, op->inputs[0].oper, cache);
            if (!input)
            {
                return nullptr;
            }
            cache.emplace(op, clone_tensor(input));
            return cache[op].get();
        }

        if (op->op_type == "Add" || op->op_type == "Mul")
        {
            if (op->inputs.size() < 2)
            {
                return nullptr;
            }

            TF_Tensor* a = self(self, op->inputs[0].oper, cache);
            TF_Tensor* b = self(self, op->inputs[1].oper, cache);
            if (!a || !b)
            {
                return nullptr;
            }

            if (a->dtype != b->dtype || a->dims != b->dims)
            {
                return nullptr;
            }

            const size_t bytes = a->owns_storage ? a->storage.size() : a->external_len;
            auto out = std::make_unique<TF_Tensor>();
            out->dtype = a->dtype;
            out->dims = a->dims;
            out->storage.resize(bytes);
            out->owns_storage = true;

            const void* pa = a->owns_storage ? static_cast<const void*>(a->storage.data()) : a->external_data;
            const void* pb = b->owns_storage ? static_cast<const void*>(b->storage.data()) : b->external_data;

            if (!pa || !pb)
            {
                return nullptr;
            }

            if (a->dtype == TF_FLOAT)
            {
                const size_t n = bytes / sizeof(float);
                const float* fa = static_cast<const float*>(pa);
                const float* fb = static_cast<const float*>(pb);
                float* fo = reinterpret_cast<float*>(out->storage.data());
                if (op->op_type == "Add")
                {
                    for (size_t i = 0; i < n; ++i) fo[i] = fa[i] + fb[i];
                }
                else
                {
                    for (size_t i = 0; i < n; ++i) fo[i] = fa[i] * fb[i];
                }
            }
            else if (a->dtype == TF_INT32)
            {
                const size_t n = bytes / sizeof(int32_t);
                const int32_t* ia = static_cast<const int32_t*>(pa);
                const int32_t* ib = static_cast<const int32_t*>(pb);
                int32_t* io = reinterpret_cast<int32_t*>(out->storage.data());
                if (op->op_type == "Add")
                {
                    for (size_t i = 0; i < n; ++i) io[i] = ia[i] + ib[i];
                }
                else
                {
                    for (size_t i = 0; i < n; ++i) io[i] = ia[i] * ib[i];
                }
            }
            else
            {
                return nullptr;
            }

            TF_Tensor* raw = out.get();
            cache.emplace(op, std::move(out));
            return raw;
        }

        return nullptr;
    };

    if (output_values && noutputs > 0)
    {
        std::unordered_map<TF_Operation*, std::unique_ptr<TF_Tensor>> cache;
        for (int i = 0; i < noutputs; ++i)
        {
            const TF_Output out = output_ops[i];
            TF_Tensor* t = eval_op(eval_op, out.oper, cache);
            if (!t)
            {
                set_status(status, TF_UNIMPLEMENTED, "TF_SessionRun: stub cannot evaluate graph");
                return;
            }

            output_values[i] = clone_tensor(t).release();
        }
    }

    set_status(status, TF_OK, "");
}

// ----------------------------------------------------------------------------
// Device list
// ----------------------------------------------------------------------------

TF_DeviceList* TF_SessionListDevices(TF_Session* session, TF_Status* status)
{
    if (!session)
    {
        set_status(status, TF_INVALID_ARGUMENT, "TF_SessionListDevices: null session");
        return nullptr;
    }

    // Return a fake device list with one CPU
    auto* list = new TF_DeviceList{};
    list->devices.push_back({"/device:CPU:0", "CPU", 0});
    
    set_status(status, TF_OK, "");
    return list;
}

void TF_DeleteDeviceList(TF_DeviceList* list)
{
    delete list;
}

int TF_DeviceListCount(const TF_DeviceList* list)
{
    return list ? static_cast<int>(list->devices.size()) : 0;
}

const char* TF_DeviceListName(const TF_DeviceList* list, int index, TF_Status* status)
{
    if (!list || index < 0 || static_cast<size_t>(index) >= list->devices.size())
    {
        set_status(status, TF_INVALID_ARGUMENT, "TF_DeviceListName: invalid index");
        return "";
    }
    
    set_status(status, TF_OK, "");
    return list->devices[static_cast<size_t>(index)].name.c_str();
}

const char* TF_DeviceListType(const TF_DeviceList* list, int index, TF_Status* status)
{
    if (!list || index < 0 || static_cast<size_t>(index) >= list->devices.size())
    {
        set_status(status, TF_INVALID_ARGUMENT, "TF_DeviceListType: invalid index");
        return "";
    }
    
    set_status(status, TF_OK, "");
    return list->devices[static_cast<size_t>(index)].type.c_str();
}

int64_t TF_DeviceListMemoryBytes(const TF_DeviceList* list, int index, TF_Status* status)
{
    if (!list || index < 0 || static_cast<size_t>(index) >= list->devices.size())
    {
        set_status(status, TF_INVALID_ARGUMENT, "TF_DeviceListMemoryBytes: invalid index");
        return 0;
    }
    
    set_status(status, TF_OK, "");
    return list->devices[static_cast<size_t>(index)].memory_bytes;
}

// ----------------------------------------------------------------------------
// SavedModel loading (stub - returns error since we can't actually load models)
// ----------------------------------------------------------------------------

TF_Session* TF_LoadSessionFromSavedModel(
    const TF_SessionOptions*,
    const TF_Buffer*,
    const char* export_dir,
    const char* const*,
    int,
    TF_Graph*,
    TF_Buffer*,
    TF_Status* status)
{
    // In stub mode, we can't actually load a SavedModel
    std::string msg = "TF_LoadSessionFromSavedModel: stub cannot load '";
    msg += export_dir ? export_dir : "(null)";
    msg += "'";
    set_status(status, TF_UNIMPLEMENTED, msg.c_str());
    return nullptr;
}
