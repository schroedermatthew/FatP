#pragma once

// ============================================================================
// TensorFlow C API shim (stub mode)
// ============================================================================
//
// When configured with:
//   -DTF_WRAPPER_TF_STUB=ON
// this project builds and runs unit tests without requiring a TensorFlow
// installation. We do that by providing:
//   1) This minimal header shim for the small subset of the TF C API that the
//      wrapper references.
//   2) A small stub implementation (see third_party/tf_stub/tf_c_stub.cpp).
//
// IMPORTANT:
// - This is NOT a complete TF C API.
// - It is not ABI/API compatible with real TensorFlow.
// - It is only intended for this wrapper's unit tests.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
// Opaque types
// ----------------------------------------------------------------------------

typedef struct TF_Status TF_Status;
typedef struct TF_Tensor TF_Tensor;
typedef struct TF_Graph TF_Graph;
typedef struct TF_Session TF_Session;
typedef struct TF_SessionOptions TF_SessionOptions;
typedef struct TF_Operation TF_Operation;
typedef struct TF_OperationDescription TF_OperationDescription;
typedef struct TF_ImportGraphDefOptions TF_ImportGraphDefOptions;
typedef struct TF_Buffer TF_Buffer;
typedef struct TF_DeviceList TF_DeviceList;

// ----------------------------------------------------------------------------
// Enums (subset)
// ----------------------------------------------------------------------------

typedef enum TF_Code {
    TF_OK = 0,
    TF_CANCELLED = 1,
    TF_UNKNOWN = 2,
    TF_INVALID_ARGUMENT = 3,
    TF_DEADLINE_EXCEEDED = 4,
    TF_NOT_FOUND = 5,
    TF_ALREADY_EXISTS = 6,
    TF_PERMISSION_DENIED = 7,
    TF_UNAUTHENTICATED = 16,
    TF_RESOURCE_EXHAUSTED = 8,
    TF_FAILED_PRECONDITION = 9,
    TF_ABORTED = 10,
    TF_OUT_OF_RANGE = 11,
    TF_UNIMPLEMENTED = 12,
    TF_INTERNAL = 13,
    TF_UNAVAILABLE = 14,
    TF_DATA_LOSS = 15
} TF_Code;

typedef enum TF_DataType {
    TF_FLOAT = 1,
    TF_DOUBLE = 2,
    TF_INT32 = 3,
    TF_UINT8 = 4,
    TF_INT16 = 5,
    TF_INT8 = 6,
    TF_STRING = 7,
    TF_COMPLEX64 = 8,
    TF_INT64 = 9,
    TF_BOOL = 10,
    TF_QINT8 = 11,
    TF_QUINT8 = 12,
    TF_QINT32 = 13,
    TF_BFLOAT16 = 14,
    TF_QINT16 = 15,
    TF_QUINT16 = 16,
    TF_UINT16 = 17,
    TF_COMPLEX128 = 18,
    TF_HALF = 19,
    TF_RESOURCE = 20,
    TF_VARIANT = 21,
    TF_UINT32 = 22,
    TF_UINT64 = 23
} TF_DataType;

// ----------------------------------------------------------------------------
// Structs (subset)
// ----------------------------------------------------------------------------

typedef struct TF_Output {
    TF_Operation* oper;
    int index;
} TF_Output;

typedef struct TF_Input {
    TF_Operation* oper;
    int index;
} TF_Input;

struct TF_Buffer {
    void* data;
    size_t length;
    void (*data_deallocator)(void* data, size_t length);
};

// ----------------------------------------------------------------------------
// Status
// ----------------------------------------------------------------------------

TF_Status* TF_NewStatus(void);
void TF_DeleteStatus(TF_Status*);
TF_Code TF_GetCode(const TF_Status*);
const char* TF_Message(const TF_Status*);
void TF_SetStatus(TF_Status*, TF_Code code, const char* msg);

// ----------------------------------------------------------------------------
// Buffer
// ----------------------------------------------------------------------------

TF_Buffer* TF_NewBuffer(void);
TF_Buffer* TF_NewBufferFromString(const void* proto, size_t proto_len);
void TF_DeleteBuffer(TF_Buffer*);

// ----------------------------------------------------------------------------
// Tensor
// ----------------------------------------------------------------------------

TF_Tensor* TF_AllocateTensor(TF_DataType, const int64_t* dims, int num_dims, size_t len);

typedef void (*TF_TensorDeallocator)(void* data, size_t len, void* arg);

TF_Tensor* TF_NewTensor(
    TF_DataType,
    const int64_t* dims,
    int num_dims,
    void* data,
    size_t len,
    TF_TensorDeallocator deallocator,
    void* deallocator_arg);

void TF_DeleteTensor(TF_Tensor*);
TF_DataType TF_TensorType(const TF_Tensor*);
void* TF_TensorData(const TF_Tensor*);
size_t TF_TensorByteSize(const TF_Tensor*);
int TF_NumDims(const TF_Tensor*);
int64_t TF_Dim(const TF_Tensor*, int dim_index);
int64_t TF_TensorElementCount(const TF_Tensor*);
size_t TF_DataTypeSize(TF_DataType);

// ----------------------------------------------------------------------------
// Graph
// ----------------------------------------------------------------------------

TF_Graph* TF_NewGraph(void);
void TF_DeleteGraph(TF_Graph*);

TF_OperationDescription* TF_NewOperation(TF_Graph*, const char* op_type, const char* oper_name);
void TF_SetDevice(TF_OperationDescription*, const char* device);
void TF_AddInput(TF_OperationDescription*, TF_Output input);
void TF_AddInputList(TF_OperationDescription*, const TF_Output* inputs, int num_inputs);
void TF_AddControlInput(TF_OperationDescription*, TF_Operation* input);
void TF_ColocateWith(TF_OperationDescription*, TF_Operation* op);

void TF_SetAttrBool(TF_OperationDescription*, const char* attr_name, unsigned char value);
void TF_SetAttrInt(TF_OperationDescription*, const char* attr_name, int64_t value);
void TF_SetAttrFloat(TF_OperationDescription*, const char* attr_name, float value);
void TF_SetAttrType(TF_OperationDescription*, const char* attr_name, TF_DataType value);
void TF_SetAttrShape(TF_OperationDescription*, const char* attr_name, const int64_t* dims, int num_dims);
void TF_SetAttrString(TF_OperationDescription*, const char* attr_name, const void* value, size_t length);
void TF_SetAttrFuncName(TF_OperationDescription*, const char* attr_name, const char* value, size_t length);

void TF_SetAttrIntList(TF_OperationDescription*, const char* attr_name, const int64_t* values, int num_values);
void TF_SetAttrFloatList(TF_OperationDescription*, const char* attr_name, const float* values, int num_values);
void TF_SetAttrTypeList(TF_OperationDescription*, const char* attr_name, const TF_DataType* values, int num_values);

void TF_SetAttrTensor(TF_OperationDescription*, const char* attr_name, TF_Tensor* value, TF_Status* status);

TF_Operation* TF_FinishOperation(TF_OperationDescription*, TF_Status* status);
void TF_DeleteOperationDescription(TF_OperationDescription*);

TF_Operation* TF_GraphOperationByName(TF_Graph*, const char* oper_name);

TF_Operation* TF_GraphNextOperation(TF_Graph*, size_t* pos);

int TF_GraphGetTensorNumDims(TF_Graph*, TF_Output, TF_Status*);
TF_DataType TF_OperationOutputType(TF_Output);

const char* TF_OperationName(TF_Operation*);
const char* TF_OperationOpType(TF_Operation*);
const char* TF_OperationDevice(TF_Operation*);
int TF_OperationNumInputs(TF_Operation*);
int TF_OperationNumOutputs(TF_Operation*);
int TF_OperationOutputNumConsumers(TF_Output);

// Graph serialization
void TF_GraphToGraphDef(TF_Graph*, TF_Buffer* output_graph_def, TF_Status* status);

// ----------------------------------------------------------------------------
// Import graph
// ----------------------------------------------------------------------------

TF_ImportGraphDefOptions* TF_NewImportGraphDefOptions(void);
void TF_DeleteImportGraphDefOptions(TF_ImportGraphDefOptions*);
void TF_ImportGraphDefOptionsSetPrefix(TF_ImportGraphDefOptions*, const char* prefix);
void TF_GraphImportGraphDef(TF_Graph*, const TF_Buffer* graph_def, const TF_ImportGraphDefOptions*, TF_Status* status);

// ----------------------------------------------------------------------------
// Session
// ----------------------------------------------------------------------------

TF_SessionOptions* TF_NewSessionOptions(void);
void TF_DeleteSessionOptions(TF_SessionOptions*);
void TF_SetTarget(TF_SessionOptions*, const char* target);
void TF_SetConfig(TF_SessionOptions*, const void* proto, size_t proto_len, TF_Status* status);

TF_Session* TF_NewSession(TF_Graph*, const TF_SessionOptions*, TF_Status* status);
void TF_CloseSession(TF_Session*, TF_Status* status);
void TF_DeleteSession(TF_Session*, TF_Status* status);

void TF_SessionRun(
    TF_Session* session,
    const TF_Buffer* run_options,
    const TF_Output* inputs,
    TF_Tensor* const* input_values,
    int ninputs,
    const TF_Output* outputs,
    TF_Tensor** output_values,
    int noutputs,
    const TF_Operation* const* target_opers,
    int ntargets,
    TF_Buffer* run_metadata,
    TF_Status* status);

// ----------------------------------------------------------------------------
// Device list (for device enumeration)
// ----------------------------------------------------------------------------

TF_DeviceList* TF_SessionListDevices(TF_Session* session, TF_Status* status);
void TF_DeleteDeviceList(TF_DeviceList* list);
int TF_DeviceListCount(const TF_DeviceList* list);
const char* TF_DeviceListName(const TF_DeviceList* list, int index, TF_Status* status);
const char* TF_DeviceListType(const TF_DeviceList* list, int index, TF_Status* status);
int64_t TF_DeviceListMemoryBytes(const TF_DeviceList* list, int index, TF_Status* status);

// ----------------------------------------------------------------------------
// SavedModel loading
// ----------------------------------------------------------------------------

TF_Session* TF_LoadSessionFromSavedModel(
    const TF_SessionOptions* session_options,
    const TF_Buffer* run_options,
    const char* export_dir,
    const char* const* tags,
    int tags_len,
    TF_Graph* graph,
    TF_Buffer* meta_graph_def,
    TF_Status* status);

#ifdef __cplusplus
} // extern "C"
#endif
