#pragma once

/*
FATP_META:
  meta_version: 1
  component: FatPTest
  file_role: test
  path: components/FatPTest/tests/test_FatP.h
  layer: Testing
  namespace: fat_p
  summary: "Forward declarations for all component test functions."
  api_stability: in_work
  related:
    docs_search: "FatP"
    headers:
      - include/fat_p/FatPTest.h
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

// Forward declarations for all component test functions
// Each function returns true on success, false on failure

bool test_AlignedVector();
bool test_AllocationStrategies();
bool test_AsyncOperations();
bool test_AtomicSharedPtr();
bool test_BinaryLite();
bool test_BitSet();
bool test_CacheUtilities();
bool test_CborLite();
bool test_CborStreamLite();
bool test_CheckedArithmetic();
bool test_CircularBuffer();
bool test_Concepts();
bool test_ConcurrencyPolicies();
bool test_ConstexprUtilities();
bool test_ContractException();
bool test_CoroutineTask();
bool test_CSRMatrix_HPC();
bool test_CSRMatrix_HPC_Parallel();
bool test_CSRMatrixParallel();
bool test_CSRMatrix();
bool test_DebugOnly();
bool test_DiagnosticLogger_Core();
bool test_DiagnosticLogger_Json();
bool test_DiagnosticLogger_IO();
bool test_DiagnosticLogger_ScopeGuard();
bool test_Enforce();
bool test_EnforcedInit();
bool test_EnhancedBoundsChecking();
bool test_EnumPlus();
bool test_EqualityComparisons();
bool test_EqualityAny();
bool test_Expected();
bool test_Factory();
bool test_FastHashMap();
bool test_FatPBenchmarkRunner();
bool test_FatPBinary();
bool test_FatPCbor();
bool test_FatPCborStream();
bool test_FatPConcepts();
bool test_FatPJson();
bool test_FatPJsonStream();
bool test_FeatureManager();
bool test_FlatMap();
bool test_FlatSet();
bool test_FloatingPointComparison();
bool test_HpcVector();
bool test_IdGenerator();
bool test_IntrusiveList();
bool test_JsonLite();
bool test_JsonStreamLite();
bool test_LockFreeQueue();
bool test_LockFreeRingBuffer();
bool test_PolicyQueue();
bool test_WorkQueue();
bool test_MemoryMappedFile();
bool test_NumaAllocator();
bool test_ObjectPool();
bool test_PipeOperator();
bool test_PolicyIterator();
bool test_RateLimiter();
bool test_RcuIntegration();
bool test_Reflection();
bool test_ScopeGuard();
bool test_ScopeGuardExpected();
bool test_ServiceLocator_HeaderSelfContained();
bool test_ServiceLocator();
bool test_Signal();
bool test_SimdVector();
bool test_SlidingFileWindow();
bool test_SlotMap();
bool test_SmallVector();
bool test_SparseSet();
bool test_StableHashMap();
bool test_Stacktrace();
bool test_StateMachine_HeaderIncludeOrder();
bool test_StateMachine_HeaderSelfContained();
bool test_StateMachine();
bool test_Stringify_HeaderSelfContained();
bool test_Stringify();
bool test_StringPool();
bool test_StrongId();
bool test_Tensor();
bool test_TensorComparison();
bool test_TensorEinsum();
bool test_TensorMath();
bool test_TensorSerializer();
bool test_TensorStorage();
bool test_ThreadPool();
bool test_ValueGuard();
bool test_ViewLifetimeTracking();
bool test_FatPTest();
