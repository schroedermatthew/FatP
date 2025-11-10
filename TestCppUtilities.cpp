// TestCppUtilities.cpp
// This file breaks tests into separate functions for each subcomponent.
// Each test includes:

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

#include "test_AdaptiveIterator.h"
#include "test_AlignedVector.h"
#include "test_AllocationStrategy.h"
#include "test_AsyncOperations.h"
#include "test_AtomicReference.h"
#include "test_BenchmarkHarness.h"
#include "test_BinarySerializer.h"
#include "test_BitSet.h"
#include "test_CacheUtilities.h"
#include "test_CheckedArithmetic.h"
#include "test_CircularBuffer.h"
#include "test_ConcurrencyPolicies.h"
#include "test_ConstexprUtilities.h"
#include "test_ContractException.h"
#include "test_CoroutineTask.h"
#include "test_CSRMatrix.h"
#include "test_DebugOnly.h"
#include "test_DiagnosticLogger.h"
#include "test_Enforce.h"
#include "test_EnforcedInit.h"
#include "test_EnumPlus.h"
#include "test_EqualityComparisons.h"
#include "test_Expected.h"
#include "test_Factory.h"
#include "test_FastHashMap.h"
#include "test_FeatureManager.h"
#include "test_FixedTensor.h"
#include "test_FlatMap.h"
#include "test_FlatSet.h"
#include "test_IdGenerator.h"
#include "test_IntrusiveList.h"
#include "test_JsonLite.h"
#include "test_LockFreeQueue.h"
#include "test_LockFreeRingBuffer.h"
#include "test_MemoryMappedFile.h"
#include "test_NumaAllocator.h"
#include "test_ObjectPool.h"
#include "test_PipeOperator.h"
#include "test_RateLimiter.h"
#include "test_Reflection.h"
#include "test_ScopeGuard.h"
#include "test_SimdVector.h"
#include "test_SlotMap.h"
#include "test_SmallVector.h"
#include "test_SortedContainer.h"
#include "test_SparseSet.h"
#include "test_Stacktrace.h"
#include "test_StateMachine.h"
#include "test_Stringify.h"
#include "test_StringPool.h"
#include "test_StrongId.h"
#include "test_Tensor.h"
#include "test_TensorCompareSeialize.h"
#include "test_TensorEinsum.h"
#include "test_TensorMath.h"
#include "test_TensorSerialization.h"
#include "test_ThreadPool.h"
#include "test_TypeTraits.h"
#include "test_UltraLoggers.h"
#include "test_Utilities.h"
#include "test_ValueGuard.h"


int main() {
    using namespace cpp_utilities::testing;

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
    RUN_AND_RECORD(test_AdaptiveIterator);
    RUN_AND_RECORD(test_AlignedVector);
    RUN_AND_RECORD(test_AllocationStrategy);
    RUN_AND_RECORD(test_AsyncOperations);
    RUN_AND_RECORD(test_AtomicReference);
    RUN_AND_RECORD(test_BenchmarkHarness);
    RUN_AND_RECORD(test_BinarySerializer);
    RUN_AND_RECORD(test_BitSet);
    RUN_AND_RECORD(test_CacheUtilities);
    RUN_AND_RECORD(test_CheckedArithmetic);
    RUN_AND_RECORD(test_CircularBuffer);
    RUN_AND_RECORD(test_ConcurrencyPolicies);
    RUN_AND_RECORD(test_ConstexprUtilities);
    RUN_AND_RECORD(test_ContractException);
    RUN_AND_RECORD(test_CoroutineTask);
    RUN_AND_RECORD(test_CSRMatrix);
    RUN_AND_RECORD(test_DebugOnly);
    RUN_AND_RECORD(test_DiagnosticLogger);
    RUN_AND_RECORD(test_Enforce);
    RUN_AND_RECORD(test_EnforcedInit);
    RUN_AND_RECORD(test_EnumPlus);
    RUN_AND_RECORD(test_EqualityComparisons);
    RUN_AND_RECORD(test_Expected);
    RUN_AND_RECORD(test_Factory);
    RUN_AND_RECORD(test_FastHashMap);
    RUN_AND_RECORD(test_FeatureManager);
    RUN_AND_RECORD(test_FixedTensor);
    RUN_AND_RECORD(test_FlatMap);
    RUN_AND_RECORD(test_FlatSet);
    RUN_AND_RECORD(test_IdGenerator);
    RUN_AND_RECORD(test_IntrusiveList);
    RUN_AND_RECORD(test_JsonLite);
    RUN_AND_RECORD(test_LockFreeQueue);
    RUN_AND_RECORD(test_LockFreeRingBuffer);
    RUN_AND_RECORD(test_MemoryMappedFile);
    RUN_AND_RECORD(test_NumaAllocator);
    RUN_AND_RECORD(test_ObjectPool);
    RUN_AND_RECORD(test_PipeOperator);
    RUN_AND_RECORD(test_RateLimiter);
    RUN_AND_RECORD(test_Reflection);
    RUN_AND_RECORD(test_ScopeGuard);
    RUN_AND_RECORD(test_SimdVector);
    RUN_AND_RECORD(test_SlotMap);
    RUN_AND_RECORD(test_SmallVector);
    RUN_AND_RECORD(test_SortedContainer);
    RUN_AND_RECORD(test_SparseSet);
    RUN_AND_RECORD(test_Stacktrace);
    RUN_AND_RECORD(test_StateMachine);
    RUN_AND_RECORD(test_Stringify);
    RUN_AND_RECORD(test_StringPool);
    RUN_AND_RECORD(test_StrongId);
    RUN_AND_RECORD(test_Tensor);
    RUN_AND_RECORD(test_TensorCompareSerialize);
    RUN_AND_RECORD(test_TensorEinsum);
    RUN_AND_RECORD(test_TensorMath);
    RUN_AND_RECORD(test_TensorSerialization);
    RUN_AND_RECORD(test_ThreadPool);
    RUN_AND_RECORD(test_TypeTraits);
    RUN_AND_RECORD(test_UltraLoggers);
    RUN_AND_RECORD(test_ValueGuard);

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