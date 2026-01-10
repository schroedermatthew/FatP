// TestCppUtilities.cpp
// This file breaks tests into separate functions for each subcomponent.
// Each test includes:

/*
FATP_META:
  meta_version: 1
  component: FatP
  file_role: test
  path: tests/test_FatP.cpp
  namespace: fat_p
  summary: "Unit tests for FatP."
  related:
    docs_search: "FatP"
    headers:
      - fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 1
    defines_unprefixed: 1
    undefs_total: 1
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <chrono> // For performance timing
#include <iostream> // For output
#include <stdexcept> // For expected exceptions
#include <string> // For strings in tests
#include <limits> // For numeric_limits (Inf/NaN, min/max)
#include <vector> // For containers
#include <array> // For fixed-size
#include <tuple> // For tuples
#include <any> // For EqualityAny
#include <mutex> // For synchronized
#include <algorithm> // For sort/unique
#include <memory> // For share_ptr
#include <numeric> // For std::iota
#include <random> // For rand() in perf tests
#include <unordered_map> // For map tests
#include <thread> // For multi-thread test

#include "IncludeAllFatPHeaders.h"

#include "test_FatP.h"

// Test of the test suite itself
#include "FatPTest.h"
#include "test_FatPTest.h"

int main() {
    using namespace fat_p::testing;

    std::cout << "Starting expanded tests for cpp_utilities..." << std::endl;

    // Structure to hold test results
    struct TestResult {
        const char* name;
        bool passed;
    };

    std::vector<TestResult> results;

    // Helper macro to run and record tests
#define RUN_AND_RECORD(test_func) \
        results.push_back({#test_func, test_func()})

    // Run all tests
    RUN_AND_RECORD(test_AlignedVector);
    RUN_AND_RECORD(test_AllocationStrategies);
    RUN_AND_RECORD(test_AsyncOperations);
    RUN_AND_RECORD(test_AtomicSharedPtr);
    RUN_AND_RECORD(test_BinaryLite);
    RUN_AND_RECORD(test_BitSet);
    RUN_AND_RECORD(test_CacheUtilities);
    RUN_AND_RECORD(test_CborLite);
    RUN_AND_RECORD(test_CborStreamLite);
    RUN_AND_RECORD(test_CheckedArithmetic);
    RUN_AND_RECORD(test_CircularBuffer);
    RUN_AND_RECORD(test_ConcurrencyPolicies);
    RUN_AND_RECORD(test_ConstexprUtilities);
    RUN_AND_RECORD(test_ContractException);
    RUN_AND_RECORD(test_CoroutineTask);
    RUN_AND_RECORD(test_CSRMatrix_HPC);
    RUN_AND_RECORD(test_CSRMatrix_HPC_Parallel);
    RUN_AND_RECORD(test_CSRMatrixParallel);
    RUN_AND_RECORD(test_CSRMatrix);
    RUN_AND_RECORD(test_DebugOnly);
    RUN_AND_RECORD(test_DiagnosticLogger_Core);
    RUN_AND_RECORD(test_DiagnosticLogger_Json);
    RUN_AND_RECORD(test_DiagnosticLogger_IO);
    RUN_AND_RECORD(test_DiagnosticLogger_ScopeGuard);
    RUN_AND_RECORD(test_Enforce);
    RUN_AND_RECORD(test_EnforcedInit);
    RUN_AND_RECORD(test_EnhancedBoundsChecking);
    RUN_AND_RECORD(test_EnumPlus);
    RUN_AND_RECORD(test_EqualityComparisons);
    RUN_AND_RECORD(test_EqualityAny);
    RUN_AND_RECORD(test_Expected);
    RUN_AND_RECORD(test_Factory);
    RUN_AND_RECORD(test_FastHashMap);
    RUN_AND_RECORD(test_FatPBenchmarkRunner);
    RUN_AND_RECORD(test_FatPBinary);
    RUN_AND_RECORD(test_FatPCbor);
    RUN_AND_RECORD(test_FatPCborStream);
    RUN_AND_RECORD(test_FatPJson);
    RUN_AND_RECORD(test_FatPJsonStream);
    RUN_AND_RECORD(test_FatPTypeTraits);
    RUN_AND_RECORD(test_FeatureManager);
    RUN_AND_RECORD(test_FlatMap);
    RUN_AND_RECORD(test_FlatSet);
    RUN_AND_RECORD(test_FloatingPointComparison);
    RUN_AND_RECORD(test_HpcVector);
    RUN_AND_RECORD(test_IdGenerator);
    RUN_AND_RECORD(test_IntrusiveList);
    RUN_AND_RECORD(test_JsonLite);
    RUN_AND_RECORD(test_JsonStreamLite);
    RUN_AND_RECORD(test_LockFreeQueue);
    RUN_AND_RECORD(test_LockFreeRingBuffer);
    RUN_AND_RECORD(test_MemoryMappedFile);
    RUN_AND_RECORD(test_NumaAllocator);
    RUN_AND_RECORD(test_ObjectPool);
    RUN_AND_RECORD(test_PipeOperator);
    RUN_AND_RECORD(test_PolicyIterator);
    RUN_AND_RECORD(test_RateLimiter);
    RUN_AND_RECORD(test_RcuIntegration);
    RUN_AND_RECORD(test_Reflection);
    RUN_AND_RECORD(test_ScopeGuard);
    RUN_AND_RECORD(test_ScopeGuardExpected);
    RUN_AND_RECORD(test_ServiceLocator);
    RUN_AND_RECORD(test_Signal);
    RUN_AND_RECORD(test_SimdVector);
    RUN_AND_RECORD(test_SlidingFileWindow);
    RUN_AND_RECORD(test_SlotMap);
    RUN_AND_RECORD(test_SmallVector);
    RUN_AND_RECORD(test_SortedContainer);
    RUN_AND_RECORD(test_SparseSet);
    RUN_AND_RECORD(test_StableHashMap);
    RUN_AND_RECORD(test_Stacktrace);
    RUN_AND_RECORD(test_StateMachine);
    RUN_AND_RECORD(test_Stringify);
    RUN_AND_RECORD(test_StringPool);
    RUN_AND_RECORD(test_StrongId);
    RUN_AND_RECORD(test_Tensor);
    RUN_AND_RECORD(test_TensorComparison);
    RUN_AND_RECORD(test_TensorEinsum);
    RUN_AND_RECORD(test_TensorMath);
    RUN_AND_RECORD(test_TensorSerializer);
    RUN_AND_RECORD(test_TensorStorage);
    RUN_AND_RECORD(test_ThreadPool);
    RUN_AND_RECORD(test_TypeTraits);
    RUN_AND_RECORD(test_ValueGuard);
    RUN_AND_RECORD(test_ViewLifetimeTracking);

    //// Test of the test suite itself
    RUN_AND_RECORD(test_FatPTest);   


#undef RUN_AND_RECORD

    // Count results
    int passed = 0;
    int failed = 0;
    std::vector<const char*> failed_tests;

    for (const auto& result : results) {
        if (result.passed) {
            ++passed;
        }
        else {
            ++failed;
            failed_tests.push_back(result.name);
        }
    }

    // Print summary
    std::cout << "\n" << colors::bold() << "========================================"
        << colors::reset() << "\n";
    std::cout << colors::bold() << "OVERALL TEST SUMMARY" << colors::reset() << "\n";
    std::cout << colors::bold() << "========================================"
        << colors::reset() << "\n";
    std::cout << colors::green() << "Passed: " << passed << colors::reset() << "\n";
    std::cout << (failed > 0 ? colors::red() : "") << "Failed: " << failed
        << colors::reset() << "\n";
    std::cout << "Total:  " << (passed + failed) << "\n";

    if (failed > 0) {
        std::cout << "\n" << colors::red() << colors::bold()
            << "FAILED TESTS:" << colors::reset() << "\n";
        for (const auto* name : failed_tests) {
            std::cout << "  " << colors::red() << name
                << colors::reset() << "\n";
        }
    }
    else {
        std::cout << "\n" << colors::green() << colors::bold()
            << "ALL TESTS PASSED!" << colors::reset() << "\n";
    }

    return failed > 0 ? 1 : 0;
}