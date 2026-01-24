#pragma once

/*
FATP_META:
  meta_version: 1
  component: FatP
  file_role: internal_header
  path: tests/test_FatP.h
  namespace: fat_p
  summary: "Test support header for FatP."
  related:
    docs_search: "FatP"
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

#include <cstdlib>
#include <string>

#ifndef ENABLE_TEST_APPLICATION

// ============================================================================
// Test Artifacts Path Utilities
// ============================================================================
//
// All test-generated non-code files (logs, data files, temporary outputs) should
// be written to the Artifacts/ subdirectory to keep the tests/ directory clean.
//
// Usage:
//   const std::string filename = fat_p::testing::artifact_file("test_output.log");
//   std::ofstream file(filename);

namespace fat_p::testing
{

namespace detail
{

// Cross-platform safe getenv wrapper
inline const char* safe_getenv(const char* name)
{
#if defined(_MSC_VER)
    // MSVC: use _dupenv_s to avoid deprecation warning
    char* buffer = nullptr;
    size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) == 0 && buffer != nullptr)
    {
        // Note: This leaks memory, but it's acceptable for test configuration
        // that's read once at startup. A more complex solution would use
        // a static string to hold the value.
        return buffer;
    }
    return nullptr;
#else
    return std::getenv(name);
#endif
}

} // namespace detail

/**
 * @brief Get the base artifacts directory path
 *
 * Returns the path to the Artifacts/ subdirectory. This can be configured
 * via the FATP_TEST_ARTIFACTS_DIR environment variable.
 *
 * @return Path to artifacts directory (with trailing separator)
 */
inline std::string artifact_path()
{
    const char* env_path = detail::safe_getenv("FATP_TEST_ARTIFACTS_DIR");
    if (env_path && env_path[0] != '\0')
    {
        std::string path = env_path;
        // Ensure trailing separator
        if (!path.empty() && path.back() != '/' && path.back() != '\\')
        {
            path += '/';
        }
        return path;
    }
    return "Artifacts/";
}

/**
 * @brief Get the full path for a file in the artifacts directory
 *
 * Combines the artifacts base path with the given filename.
 *
 * @param filename The filename (without path) to place in artifacts
 * @return Full path to the file in the artifacts directory
 *
 * @example
 *   const std::string path = artifact_file("test_output.log");
 *   // Returns "Artifacts/test_output.log" (or custom path if env var set)
 */
inline std::string artifact_file(const std::string& filename)
{
    return artifact_path() + filename;
}

} // namespace fat_p::testing

// Forward declarations for test functions
namespace fat_p::testing
{

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
bool test_ConcurrencyPolicies();
bool test_ConstexprUtilities();
bool test_ContractException();
bool test_CoroutineTask();
bool test_CSRMatrix();
bool test_CSRMatrix_HPC();
bool test_CSRMatrix_HPC_Parallel();
bool test_CSRMatrixParallel();
bool test_DebugOnly();
bool test_DiagnosticLogger_Core();
bool test_DiagnosticLogger_IO();
bool test_DiagnosticLogger_Json();
bool test_DiagnosticLogger_ScopeGuard();
bool test_Enforce();
bool test_EnforcedInit();
bool test_EnhancedBoundsChecking();
bool test_EnumPlus();
bool test_EqualityAny();
bool test_EqualityComparisons();
bool test_Expected();
bool test_Factory();
bool test_FastHashMap();
bool test_FatPBenchmarkRunner();
bool test_FatPBinary();
bool test_FatPCbor();
bool test_FatPCborStream();
bool test_FatPJson();
bool test_FatPJsonStream();
bool test_FatPTypeTraits();
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
bool test_ServiceLocator();
bool test_Signal();
bool test_SimdVector();
bool test_SlidingFileWindow();
bool test_SlotMap();
bool test_SmallVector();
bool test_SortedContainer();
bool test_SparseSet();
bool test_StableHashMap();
bool test_Stacktrace();
bool test_StateMachine();
bool test_StateMachine_HeaderIncludeOrder();
bool test_StateMachine_HeaderSelfContained(); 
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
bool test_TypeTraits();
bool test_ValueGuard();
bool test_ViewLifetimeTracking();

} // namespace fat_p::testing
#endif // #ifndef ENABLE_TEST_APPLICATION
