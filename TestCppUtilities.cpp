// TestCppUtilities.cpp (Expanded with Thorough Tests, Corner Cases, and Performance Evaluation)
// This file now breaks tests into separate functions for each subcomponent.
// Each test includes:
// - Nominal cases: Basic usage as before.
// - Corner cases: Edge scenarios like null pointers, overflows, empty containers, NaN/Inf for floats, invalid states.
// - Error handling: Expected throws/caught exceptions.
// - Performance evaluation: Uses std::chrono for timing comparisons (e.g., custom vs standard STL).
// No external deps (e.g., no Google Test); uses simple assert-like macros and std::cout for output.
// Compile with C++17; run in debug/release for perf diffs.
// To measure perf accurately, compile in release mode (/O2 or -O3) and run multiple iterations.

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

// All library includes (ensure order to avoid dependency issues)
#include "test_AdaptiveIterator.h"
#include "test_AllocationStrategy.h"
#include "test_AtomicReference.h"
#include "test_CheckedArithmetic.h"
#include "test_ConcurrencyPolicies.h"
#include "test_ConstexprUtilities.h"
#include "test_DiagnosticLogger.h"
#include "test_EnforcedInit.h"
#include "test_Enforce.h"
#include "test_EnumPlus.h"
#include "test_EqualityComparisons.h"
#include "test_Expected.h"
#include "test_Factory.h"
#include "test_JsonLite.h"
#include "test_ScopeGuard.h"
#include "test_SortedContainer.h"
#include "test_StateMachine.h"
#include "test_Stringify.h"
#include "test_StrongId.h"
#include "test_TypeTraits.h"
#include "test_Utilities.h"
#include "test_UltraLoggers.h"
#include "test_ValueGuard.h"

namespace cpp_utilities::testing
{

} // namespace cpp_utilities::testing;

int main() {
    using namespace cpp_utilities::testing;

    std::cout << "Starting expanded tests for cpp_utilities..." << std::endl;
    bool all_passed = true;
    all_passed &= test_AdaptiveIterator();
    all_passed &= test_AllocationStrategy();
    all_passed &= test_AtomicReference();
    all_passed &= test_CheckedArithmetic();
    all_passed &= test_ConcurrencyPolicies();
    all_passed &= test_ConstexprUtilities();
    all_passed &= test_DiagnosticLogger();
    all_passed &= test_Enforce();
    all_passed &= test_EnforcedInit();
    all_passed &= test_EnumPlus();
    all_passed &= test_EqualityComparisons();
    all_passed &= test_Expected();
    all_passed &= test_Factory();
    all_passed &= test_JsonLite();
    all_passed &= test_ScopeGuard();
    all_passed &= test_StateMachine();
    all_passed &= test_Stringify();
    all_passed &= test_StrongId();
    all_passed &= test_SortedContainer();
    all_passed &= test_TypeTraits();
    all_passed &= test_ValueGuard();
    all_passed &= test_UltraLoggers();


    if (all_passed) {
        std::cout << "All expanded tests passed!" << std::endl;
    }
    else {
        std::cout << "Some tests failed!" << std::endl;
    }
    return all_passed ? 0 : 1;
}