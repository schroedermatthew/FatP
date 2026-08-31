#!/usr/bin/env python3

# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/generate_workflows.py
#   summary: "Generates all Fat-P CI workflow files from canonical templates."
#   api_stability: in_work
#   layer: Infrastructure
#   related:
#     docs_search: ""
#     tests: []
#   hygiene:
#     pragma_once: false
#     include_guard: false
#     defines_total: 0
#     defines_unprefixed: 0
#     undefs_total: 0
#     includes_windows_h: false
"""
Generate all Fat-P CI workflow files from canonical templates.
Based on object-pool.yml as the reference implementation.
"""

import os

OUTPUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), ".github", "workflows")

# GitHub-hosted runners use Node 24 for JavaScript actions (Node 20 deprecated 2026).
# checkout@v6+, cache@v5+, upload/download-artifact@v5+ run on Node 24.

# sccache via GHA backend — speeds up FatP aggregate (~90 TU) rebuilds.
SCCACHE_ACTION = "mozilla-actions/sccache-action@v0.0.9"

# =============================================================================
# Component definitions
# =============================================================================
# Each tuple: (workflow_filename, component_name, header, test_src, bench_src_or_None)
# For Linux-only components, we use a separate list.

# Extra link libraries for GCC C++23 builds (e.g. std::stacktrace in libstdc++exp).
COMPONENT_GCC_CPP23_EXTRA_LIBS = {
    "Stacktrace": "-lstdc++exp",
    "FatP": "-lstdc++exp -pthread",
}

FATP_AGGREGATE_WORKFLOW = "fatp-test-core.yml"

# Public facades stay flat, while these components own implementation headers
# in grouped subdirectories. Keep both paths in the component trigger so a
# facade-only API and its implementation cannot drift outside CI coverage.
EXTRA_TRIGGER_PATHS = {
    "policy-iterator.yml": [
        "include/fat_p/TensorIteration.h",
        "include/fat_p/TensorStridePolicy.h",
        "include/fat_p/policy_iterator/**",
        "components/PolicyIterator/docs/*TensorStridePolicy.md",
        "components/PolicyIterator/tests/test_TensorIteration_HeaderSelfContained.cpp",
        "components/PolicyIterator/tests/test_TensorStridePolicy_HeaderSelfContained.cpp",
    ],
    "tensor.yml": [
        "include/fat_p/AlignedVector.h",
        "include/fat_p/tensor/Tensor.h",
        "include/fat_p/tensor/TensorExtents.h",
        "include/fat_p/tensor/TensorLayout.h",
        "include/fat_p/tensor/TensorView.h",
        "include/fat_p/tensor/TensorIterationPlan.h",
        "include/fat_p/tensor/TensorKernels.h",
    ],
    "tensor-layout.yml": [
        "include/fat_p/tensor/TensorExtents.h",
        "include/fat_p/tensor/TensorLayout.h",
    ],
    "tensor-view.yml": [
        "include/fat_p/tensor/TensorView.h",
        "include/fat_p/tensor/TensorExtents.h",
        "include/fat_p/tensor/TensorLayout.h",
    ],
    "tensor-algorithms.yml": [
        "include/fat_p/tensor/TensorAlgorithms.h",
        "include/fat_p/tensor/TensorIterationPlan.h",
        "include/fat_p/tensor/TensorKernels.h",
        "include/fat_p/tensor/Tensor.h",
        "include/fat_p/tensor/TensorView.h",
        "include/fat_p/tensor/TensorLayout.h",
        "include/fat_p/tensor/TensorExtents.h",
    ],
    "tensor-slice.yml": [
        "include/fat_p/tensor/TensorSlice.h",
        "include/fat_p/tensor/TensorView.h",
        "include/fat_p/tensor/TensorLayout.h",
        "include/fat_p/tensor/TensorExtents.h",
        "include/fat_p/tensor/Tensor.h",
    ],
    "tensor-reductions.yml": [
        "include/fat_p/tensor/TensorReductions.h",
        "include/fat_p/tensor/TensorAlgorithms.h",
        "include/fat_p/tensor/TensorIterationPlan.h",
        "include/fat_p/tensor/TensorKernels.h",
        "include/fat_p/tensor/Tensor.h",
        "include/fat_p/tensor/TensorView.h",
        "include/fat_p/tensor/TensorLayout.h",
        "include/fat_p/tensor/TensorExtents.h",
    ],
    "tensor-interop.yml": [
        "include/fat_p/tensor/TensorInterop.h",
        "include/fat_p/tensor/TensorStatic.h",
        "include/fat_p/tensor/Tensor.h",
        "include/fat_p/tensor/TensorView.h",
        "include/fat_p/tensor/TensorLayout.h",
        "include/fat_p/tensor/TensorExtents.h",
    ],
    "tensor-matmul.yml": [
        "include/fat_p/tensor/TensorMatmul.h",
        "include/fat_p/tensor/TensorReductions.h",
        "include/fat_p/tensor/TensorAlgorithms.h",
        "include/fat_p/tensor/TensorIterationPlan.h",
        "include/fat_p/tensor/TensorKernels.h",
        "include/fat_p/tensor/Tensor.h",
        "include/fat_p/tensor/TensorView.h",
        "include/fat_p/tensor/TensorLayout.h",
        "include/fat_p/tensor/TensorExtents.h",
    ],
    "tensor-selection.yml": [
        "include/fat_p/tensor/TensorSelection.h",
        "include/fat_p/tensor/TensorAlgorithms.h",
        "include/fat_p/tensor/Tensor.h",
        "include/fat_p/tensor/TensorView.h",
        "include/fat_p/tensor/TensorLayout.h",
        "include/fat_p/tensor/TensorExtents.h",
    ],
    "tensor-equality.yml": [
        "include/fat_p/tensor/TensorEquality.h",
        "include/fat_p/tensor/TensorKernels.h",
        "include/fat_p/tensor/TensorIterationPlan.h",
        "include/fat_p/tensor/Tensor.h",
        "include/fat_p/tensor/TensorView.h",
        "include/fat_p/tensor/TensorLayout.h",
        "include/fat_p/tensor/TensorExtents.h",
    ],
    "tensor-static.yml": ["include/fat_p/tensor/TensorStatic.h"],
    "tensor-serializer.yml": [
        "include/fat_p/tensor/TensorSerializer.h",
        "include/fat_p/tensor/Tensor.h",
        "include/fat_p/tensor/TensorView.h",
        "include/fat_p/tensor/TensorLayout.h",
        "include/fat_p/tensor/TensorExtents.h",
    ],
}

# Tensor's semantic contract requires assertions-enabled Debug coverage in
# addition to the ordinary optimized matrix. Keep this opt-in so the generated
# workflow cost is deliberate rather than silently multiplied for every
# component.
DEBUG_WORKFLOWS = {
    "tensor.yml",
    "tensor-layout.yml",
    "tensor-view.yml",
    "tensor-algorithms.yml",
    "tensor-slice.yml",
    "tensor-reductions.yml",
    "tensor-interop.yml",
    "tensor-matmul.yml",
    "tensor-selection.yml",
}

# Sanitizer sources are compiled and executed one translation unit at a time.
# Future Tensor layout/kernel conformance files belong in this list so they do
# not accidentally miss pointer and signed-offset instrumentation.
SANITIZER_TEST_SOURCES = {
    "tensor.yml": [
        "components/Tensor/tests/test_Tensor.cpp",
        "components/Tensor/tests/test_TensorLayout.cpp",
        "components/Tensor/tests/test_TensorView.cpp",
        "components/Tensor/tests/test_TensorAlgorithms.cpp",
        "components/Tensor/tests/test_TensorSlice.cpp",
        "components/Tensor/tests/test_TensorReductions.cpp",
        "components/Tensor/tests/test_TensorInterop.cpp",
        "components/Tensor/tests/test_TensorMatmul.cpp",
        "components/Tensor/tests/test_TensorSelection.cpp",
    ],
}

ADDITIONAL_HEADER_CHECKS = {
    "policy-iterator.yml": ["TensorIteration.h", "TensorStridePolicy.h"],
}

STANDARD_COMPONENTS = [
    # =========================================================================
    # Components WITH benchmarks
    # =========================================================================
    ("aligned-vector.yml", "AlignedVector", "AlignedVector.h",
     "components/AlignedVector/tests/test_AlignedVector.cpp",
     "components/AlignedVector/benchmarks/benchmark_AlignedVector.cpp"),

    ("allocation-strategies.yml", "AllocationStrategies", "AllocationStrategies.h",
     "components/AllocationStrategies/tests/test_AllocationStrategies.cpp",
     "components/AllocationStrategies/benchmarks/benchmark_AllocationStrategies.cpp"),

    ("bit-set.yml", "BitSet", "BitSet.h",
     "components/BitSet/tests/test_BitSet.cpp",
     "components/BitSet/benchmarks/benchmark_BitSet.cpp"),

    ("circular-buffer.yml", "CircularBuffer", "CircularBuffer.h",
     "components/CircularBuffer/tests/test_CircularBuffer.cpp",
     "components/CircularBuffer/benchmarks/benchmark_CircularBuffer.cpp"),

    ("feature-manager.yml", "FeatureManager", "FeatureManager.h",
     "components/FeatureManager/tests/test_FeatureManager.cpp",
     "components/FeatureManager/benchmarks/benchmark_FeatureManager.cpp"),

    ("flat-map.yml", "FlatMap", "FlatMap.h",
     "components/FlatMapSet/tests/test_FlatMap.cpp",
     "components/FlatMapSet/benchmarks/benchmark_FlatMapSet.cpp"),

    ("floating-point-comparison.yml", "FloatingPointComparison", "FloatingPointComparison.h",
     "components/FloatingPointComparison/tests/test_FloatingPointComparison.cpp",
     "components/FloatingPointComparison/benchmarks/benchmark_FloatingPointComparison.cpp"),

    ("intrusive-list.yml", "IntrusiveList", "IntrusiveList.h",
     "components/IntrusiveList/tests/test_IntrusiveList.cpp",
     "components/IntrusiveList/benchmarks/benchmark_IntrusiveList.cpp"),

    ("object-pool.yml", "ObjectPool", "ObjectPool.h",
     "components/ObjectPool/tests/test_ObjectPool.cpp",
     "components/ObjectPool/benchmarks/benchmark_ObjectPool.cpp"),

    ("policy-iterator.yml", "PolicyIterator", "PolicyIterator.h",
     "components/PolicyIterator/tests/test_PolicyIterator.cpp",
     "components/PolicyIterator/benchmarks/benchmark_PolicyIterator.cpp"),

    ("slot-map.yml", "SlotMap", "SlotMap.h",
     "components/SlotMap/tests/test_SlotMap.cpp",
     "components/SlotMap/benchmarks/benchmark_SlotMap.cpp"),

    ("small-vector.yml", "SmallVector", "SmallVector.h",
     "components/SmallVector/tests/test_SmallVector.cpp",
     "components/SmallVector/benchmarks/benchmark_SmallVector.cpp"),

    ("sparse-set.yml", "SparseSet", "SparseSet.h",
     "components/SparseSet/tests/test_SparseSet.cpp",
     "components/SparseSet/benchmarks/benchmark_SparseSet.cpp"),

    ("stacktrace.yml", "Stacktrace", "Stacktrace.h",
     "components/Stacktrace/tests/test_Stacktrace.cpp",
     "components/Stacktrace/benchmarks/benchmark_Stacktrace.cpp"),

    ("stringify.yml", "Stringify", "Stringify.h",
     "components/Stringify/tests/test_Stringify.cpp",
     "components/Stringify/benchmarks/benchmark_Stringify.cpp"),

    ("strong-id.yml", "StrongId", "StrongId.h",
     "components/StrongId/tests/test_StrongId.cpp",
     "components/StrongId/benchmarks/benchmark_StrongId.cpp"),

    ("service-locator.yml", "ServiceLocator", "ServiceLocator.h",
     "components/ServiceLocator/tests/test_ServiceLocator.cpp",
     "components/ServiceLocator/benchmarks/benchmark_ServiceLocator.cpp"),

    # =========================================================================
    # Components WITHOUT benchmarks
    # =========================================================================

    # --- BinarySerialization ---
    ("binary-lite.yml", "BinaryLite", "BinaryLite.h",
     "components/BinarySerialization/tests/test_BinaryLite.cpp", None),

    ("fatp-binary.yml", "FatPBinary", "FatPBinary.h",
     "components/BinarySerialization/tests/test_FatPBinary.cpp", None),

    # --- Cbor ---
    ("cbor-lite.yml", "CborLite", "CborLite.h",
     "components/Cbor/tests/test_CborLite.cpp", None),

    ("cbor-stream-lite.yml", "CborStreamLite", "CborStreamLite.h",
     "components/Cbor/tests/test_CborStreamLite.cpp", None),

    ("fatp-cbor.yml", "FatPCbor", "FatPCbor.h",
     "components/Cbor/tests/test_FatPCbor.cpp", None),

    ("fatp-cbor-stream.yml", "FatPCborStream", "FatPCborStream.h",
     "components/Cbor/tests/test_FatPCborStream.cpp", None),

    # --- Json ---
    ("fatp-json.yml", "FatPJson", "FatPJson.h",
     "components/Json/tests/test_FatPJson.cpp", None),

    ("fatp-json-stream.yml", "FatPJsonStream", "FatPJsonStream.h",
     "components/Json/tests/test_FatPJsonStream.cpp", None),

    ("json-lite.yml", "JsonLite", "JsonLite.h",
     "components/Json/tests/test_JsonLite.cpp", None),

    ("json-stream-lite.yml", "JsonStreamLite", "JsonStreamLite.h",
     "components/Json/tests/test_JsonStreamLite.cpp", None),

    ("xml-lite.yml", "XmlLite", "XmlLite.h",
     "components/Xml/tests/test_XmlLite.cpp", None),

    # --- Tensor ---
    ("tensor.yml", "Tensor", "Tensor.h",
     "components/Tensor/tests/test_Tensor.cpp", "components/Tensor/benchmarks/benchmark_Tensor.cpp"),

    ("tensor-layout.yml", "TensorLayout", "TensorLayout.h",
     "components/Tensor/tests/test_TensorLayout.cpp", None),

    ("tensor-view.yml", "TensorView", "TensorView.h",
     "components/Tensor/tests/test_TensorView.cpp", None),

    ("tensor-algorithms.yml", "TensorAlgorithms", "TensorAlgorithms.h",
     "components/Tensor/tests/test_TensorAlgorithms.cpp", None),

    ("tensor-slice.yml", "TensorSlice", "TensorSlice.h",
     "components/Tensor/tests/test_TensorSlice.cpp", None),

    ("tensor-reductions.yml", "TensorReductions", "TensorReductions.h",
     "components/Tensor/tests/test_TensorReductions.cpp", None),

    ("tensor-interop.yml", "TensorInterop", "TensorInterop.h",
     "components/Tensor/tests/test_TensorInterop.cpp", None),

    ("tensor-matmul.yml", "TensorMatmul", "TensorMatmul.h",
     "components/Tensor/tests/test_TensorMatmul.cpp", "components/Tensor/benchmarks/benchmark_TensorMatmul.cpp"),

    ("tensor-selection.yml", "TensorSelection", "TensorSelection.h",
     "components/Tensor/tests/test_TensorSelection.cpp", None),

    ("tensor-equality.yml", "TensorEquality", "TensorEquality.h",
     "components/Tensor/tests/test_TensorEquality.cpp", None),


    ("tensor-static.yml", "TensorStatic", "TensorStatic.h",
     "components/Tensor/tests/test_TensorStatic.cpp", None),

    ("tensor-serializer.yml", "TensorSerializer", "TensorSerializer.h",
     "components/Tensor/tests/test_TensorSerializer.cpp", None),

    # --- CSRMatrix (4 test files) ---
    ("csr-matrix.yml", "CSRMatrix", "CSRMatrix.h",
     "components/CSRMatrix/tests/test_CSRMatrix.cpp", None),

    ("csr-matrix-parallel.yml", "CSRMatrixParallel", "CSRMatrixParallel.h",
     "components/CSRMatrix/tests/test_CSRMatrixParallel.cpp", None),

    ("csr-matrix-hpc.yml", "CSRMatrix_HPC", "CSRMatrix_HPC.h",
     "components/CSRMatrix/tests/test_CSRMatrix_HPC.cpp", None),

    ("csr-matrix-hpc-parallel.yml", "CSRMatrix_HPC_Parallel", "CSRMatrix_HPC.h",
     "components/CSRMatrix/tests/test_CSRMatrix_HPC_Parallel.cpp", None),

    # --- DiagnosticLogger (4 test files) ---
    ("diagnostic-logger-core.yml", "DiagnosticLogger_Core", "DiagnosticLogger_Core.h",
     "components/DiagnosticLogger/tests/test_DiagnosticLogger_Core.cpp", None),

    ("diagnostic-logger-io.yml", "DiagnosticLogger_IO", "DiagnosticLogger_IO.h",
     "components/DiagnosticLogger/tests/test_DiagnosticLogger_IO.cpp", None),

    ("diagnostic-logger-json.yml", "DiagnosticLogger_Json", "DiagnosticLogger_Json.h",
     "components/DiagnosticLogger/tests/test_DiagnosticLogger_Json.cpp", None),

    ("diagnostic-logger-scopeguard.yml", "DiagnosticLogger_ScopeGuard", "DiagnosticLogger_IO.h",
     "components/DiagnosticLogger/tests/test_DiagnosticLogger_ScopeGuard.cpp", None),

    # --- Concepts ---
    ("concepts.yml", "Concepts", "Concepts.h",
     "components/Concepts/tests/test_Concepts.cpp", None),

    ("fatp-concepts.yml", "FatPConcepts", "FatPConcepts.h",
     "components/Concepts/tests/test_FatPConcepts.cpp", None),

    # --- ConcurrencyPolicies ---
    ("concurrency-policies.yml", "ConcurrencyPolicies", "ConcurrencyPolicies.h",
     "components/ConcurrencyPolicies/tests/test_ConcurrencyPolicies.cpp", None),

    ("rcu-integration.yml", "RcuIntegration", "ConcurrencyPolicies.h",
     "components/ConcurrencyPolicies/tests/test_RcuIntegration.cpp", None),

    # --- Equality ---
    ("equality-any.yml", "EqualityAny", "EqualityAny.h",
     "components/Equality/tests/test_EqualityAny.cpp", None),

    ("equality-comparisons.yml", "EqualityComparisons", "EqualityComparisons.h",
     "components/Equality/tests/test_EqualityComparisons.cpp", None),

    # --- FatPTest ---
    ("fatp-test.yml", "FatPTest", "FatPTest.h",
     "components/FatPTest/tests/test_FatPTest.cpp", None),

    ("fatp-test-core.yml", "FatP", "FatPTest.h",
     "components/FatPTest/tests/test_FatP.cpp", None),

    # --- FlatMapSet (FlatSet has no benchmarks, FlatMap handled above) ---
    ("flat-set.yml", "FlatSet", "FlatSet.h",
     "components/FlatMapSet/tests/test_FlatSet.cpp", None),

    # --- LockFreeContainers ---
    ("lock-free-queue.yml", "LockFreeQueue", "LockFreeQueue.h",
     "components/LockFreeContainers/tests/test_LockFreeQueue.cpp", None),

    ("lock-free-ring-buffer.yml", "LockFreeRingBuffer", "LockFreeRingBuffer.h",
     "components/LockFreeContainers/tests/test_LockFreeRingBuffer.cpp", None),

    # --- ScopeGuard ---
    ("scope-guard.yml", "ScopeGuard", "ScopeGuard.h",
     "components/ScopeGuard/tests/test_ScopeGuard.cpp", None),

    ("scope-guard-expected.yml", "ScopeGuardExpected", "ScopeGuardExpected.h",
     "components/ScopeGuard/tests/test_ScopeGuardExpected.cpp", None),

    # --- Single-test components (previously existing) ---
    ("hpc-vector.yml", "HpcVector", "HpcVector.h",
     "components/HpcVector/tests/test_HpcVector.cpp", None),

    ("id-generator.yml", "IdGenerator", "IdGenerator.h",
     "components/IdGenerator/tests/test_IdGenerator.cpp", None),

    ("rate-limiter.yml", "RateLimiter", "RateLimiter.h",
     "components/RateLimiter/tests/test_RateLimiter.cpp", None),

    ("reflection.yml", "Reflection", "Reflection.h",
     "components/Reflection/tests/test_Reflection.cpp", None),

    ("signal.yml", "Signal", "Signal.h",
     "components/Signal/tests/test_Signal.cpp", None),

    ("simd-vector.yml", "SimdVector", "SimdVector.h",
     "components/SimdVector/tests/test_SimdVector.cpp", None),

    ("string-pool.yml", "StringPool", "StringPool.h",
     "components/StringPool/tests/test_StringPool.cpp", None),

    ("thread-pool.yml", "ThreadPool", "ThreadPool.h",
     "components/ThreadPool/tests/test_ThreadPool.cpp", None),

    ("value-guard.yml", "ValueGuard", "ValueGuard.h",
     "components/ValueGuard/tests/test_ValueGuard.cpp", None),

    ("view-lifetime-tracking.yml", "ViewLifetimeTracking", "ViewLifetimeTracking.h",
     "components/ViewLifetimeTracking/tests/test_ViewLifetimeTracking.cpp", None),

    # --- Single-test components (previously missing, Gap 2) ---
    ("async-operations.yml", "AsyncOperations", "ExpectedAsyncTask.h",
     "components/Expected/tests/test_AsyncOperations.cpp", None),

    ("cache-utilities.yml", "CacheUtilities", "CacheUtilities.h",
     "components/CacheUtilities/tests/test_CacheUtilities.cpp", None),

    ("checked-arithmetic.yml", "CheckedArithmetic", "CheckedArithmetic.h",
     "components/CheckedArithmetic/tests/test_CheckedArithmetic.cpp", None),

    ("constexpr-utilities.yml", "ConstexprUtilities", "ConstexprUtilities.h",
     "components/ConstexprUtilities/tests/test_ConstexprUtilities.cpp", None),

    ("contract-exception.yml", "ContractException", "ContractException.h",
     "components/ContractException/tests/test_ContractException.cpp", None),

    ("coroutine-task.yml", "CoroutineTask", "CoroutineTask.h",
     "components/CoroutineTask/tests/test_CoroutineTask.cpp", None),

    ("debug-only.yml", "DebugOnly", "DebugOnly.h",
     "components/DebugOnly/tests/test_DebugOnly.cpp", None),

    ("enforce.yml", "Enforce", "enforce.h",
     "components/Enforce/tests/test_Enforce.cpp", None),

    ("enforced-init.yml", "EnforcedInit", "EnforcedInit.h",
     "components/EnforcedInit/tests/test_EnforcedInit.cpp", None),

    ("enhanced-bounds-checking.yml", "EnhancedBoundsChecking", "EnhancedBoundsChecking.h",
     "components/EnhancedBoundsChecking/tests/test_EnhancedBoundsChecking.cpp", None),

    ("enum-plus.yml", "EnumPlus", "EnumPlus.h",
     "components/EnumPlus/tests/test_EnumPlus.cpp", None),

    ("expected.yml", "Expected", "Expected.h",
     "components/Expected/tests/test_Expected.cpp", None),

    ("factory.yml", "Factory", "Factory.h",
     "components/Factory/tests/test_Factory.cpp", None),

    ("fatp-benchmark-runner.yml", "FatPBenchmarkRunner", "FatPBenchmarkRunner.h",
     "components/FatPBenchmarkRunner/tests/test_FatPBenchmarkRunner.cpp", None),

    ("work-queue.yml", "WorkQueue", "WorkQueue.h",
     "components/WorkQueue/tests/test_WorkQueue.cpp", None),
]

# Linux-only components (no MSVC job)
LINUX_ONLY_COMPONENTS = [
    ("memory-mapped-file.yml", "MemoryMappedFile", "MemoryMappedFile.h",
     "components/MemoryMappedFile/tests/test_MemoryMappedFile.cpp", None),

    ("numa-allocator.yml", "NumaAllocator", "NumaAllocator.h",
     "components/NumaAllocator/tests/test_NumaAllocator.cpp", None),

    ("sliding-file-window.yml", "SlidingFileWindow", "SlidingFileWindow.h",
     "components/SlidingFileWindow/tests/test_SlidingFileWindow.cpp", None),
]


def backslash_path(p):
    """Convert forward-slash path to backslash for MSVC."""
    return p.replace("/", "\\")


def generate_header_block(filename, component, header, test_src, bench_src):
    """Generate the file header comment block."""
    lines = [
        f"# =============================================================================",
        f"# .github/workflows/{filename}",
        f"# =============================================================================",
        f"# CI workflow for {component} component",
        f"#",
        f"# Directory structure:",
        f"#   Headers:    include/fat_p/{header}",
        f"#   Tests:      {test_src}",
    ]
    if bench_src:
        lines.append(f"#   Benchmarks: {bench_src}")
        lines.append("#   Execution:  separate manual benchmark workflow")
    lines.append(f"# =============================================================================")
    return "\n".join(lines)


def generate_aggregate_trigger_block(filename):
    """Trigger when any aggregate test input changes."""
    paths = [
        "include/fat_p/FatPTest.h",
        "components/FatPTest/tests/test_FatP.cpp",
        "components/FatPTest/tests/test_FatP.h",
        "components/**/tests/test_*.cpp",
        "tools/collect_fatp_test_sources.py",
        f".github/workflows/{filename}",
    ]
    path_yaml = "\n".join(f"      - '{p}'" for p in paths)
    return f"""
on:
  workflow_dispatch:
  push:
    paths:
{path_yaml}
  pull_request:
    paths:
{path_yaml}"""


def sccache_setup_step():
    return f"""
      - name: Setup sccache
        uses: {SCCACHE_ACTION}"""


def sccache_stats_step():
    return """
      - name: sccache statistics
        if: always()
        run: sccache --show-stats"""


def aggregate_unix_build(compiler_line, extra_flags="", link_setup=""):
    """Build all component tests for test_FatP.cpp without standalone mains."""
    link_block = link_setup or '          LINK_EXTRA=""'
    extra = f"            {extra_flags}\n" if extra_flags else ""
    return f"""{link_block}
          mapfile -t TEST_SRCS < <(python tools/collect_fatp_test_sources.py)
          {compiler_line} \\
            -Wall -Wextra -Wpedantic -Werror \\
{extra}            -O2 -DNDEBUG \\
            -I./include/fat_p \\
            "${{TEST_SRCS[@]}}" -o test_bin $LINK_EXTRA"""


def generate_trigger_block(filename, header, test_src, bench_src):
    """Generate the on: trigger block with push/pull_request path triggers."""
    # Unit test .cpp changes are covered by fatp-test-core.yml (aggregate link +
    # full suite). Listing test_src here too would fire this workflow on every
    # test edit alongside FatP CI — duplicate Actions runs for the same push.
    paths = [
        f"include/fat_p/{header}",
    ]
    paths.extend(EXTRA_TRIGGER_PATHS.get(filename, []))
    if bench_src:
        paths.append(bench_src)
    paths.append(f".github/workflows/{filename}")

    path_yaml = "\n".join(f"      - '{p}'" for p in paths)

    return f"""
on:
  workflow_dispatch:
  push:
    paths:
{path_yaml}
  pull_request:
    paths:
{path_yaml}"""


def generate_env_block(header, test_src, sanitizer_sources=None):
    """Generate the env: block."""
    lines = [
        "",
        "env:",
        f"  HEADER: {header}",
        f"  TEST_SRC: {test_src}",
    ]
    if sanitizer_sources:
        lines.append(f"  SANITIZER_TEST_SRCS: {' '.join(sanitizer_sources)}")
    return "\n".join(lines)


def generate_debug_jobs(test_src, include_msvc=True):
    """Generate assertions-enabled C++20 jobs for contract-heavy components."""
    bs_test = backslash_path(test_src)
    msvc_job = ""
    if include_msvc:
        msvc_job = f"""

  debug-windows-msvc:
    name: Debug Windows MSVC C++20
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v6
      - name: Setup MSVC
        uses: TheMrMilchmann/setup-msvc-dev@v4
        with:
          arch: x64
      - name: Build assertions-enabled tests
        shell: cmd
        run: cl /std:c++20 /W4 /WX /wd4324 /wd4127 /EHsc /permissive- /Zc:preprocessor /Od /Zi /DENABLE_TEST_APPLICATION /I.\\include\\fat_p {bs_test} /Fe:test_debug.exe /link advapi32.lib
      - name: Run tests
        run: .\\test_debug.exe"""

    return f"""
  # ===========================================================================
  # Assertions-enabled Debug builds (C++20)
  # ===========================================================================
  debug-linux-gcc:
    name: Debug Linux GCC-13 C++20
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v6
      - name: Build assertions-enabled tests
        run: |
          g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror \\
            -O0 -g3 -DENABLE_TEST_APPLICATION -I./include/fat_p \\
            ${{{{ env.TEST_SRC }}}} -o test_debug
      - name: Run tests
        run: ./test_debug

  debug-linux-clang:
    name: Debug Linux Clang-16 C++20
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v6
      - name: Install Clang
        run: |
          wget https://apt.llvm.org/llvm.sh
          chmod +x llvm.sh
          sudo ./llvm.sh 16
      - name: Build assertions-enabled tests
        run: |
          clang++-16 -std=c++20 -Wall -Wextra -Wpedantic -Werror \\
            -Wno-gnu-zero-variadic-macro-arguments \\
            -O0 -g3 -DENABLE_TEST_APPLICATION -I./include/fat_p \\
            ${{{{ env.TEST_SRC }}}} -o test_debug
      - name: Run tests
        run: ./test_debug{msvc_job}"""


def generate_linux_gcc_job(gcc_cpp23_extra_libs="", aggregate=False):
    link_setup = "          LINK_EXTRA=\"\""
    if gcc_cpp23_extra_libs:
        link_setup = f"""          LINK_EXTRA=""
          if [ "${{{{ matrix.std }}}}" = "23" ]; then
            LINK_EXTRA="{gcc_cpp23_extra_libs}"
          fi"""
    if aggregate:
        compiler = "sccache g++-${{ matrix.version }} -std=c++${{ matrix.std }}"
        build_body = aggregate_unix_build(compiler, link_setup=link_setup)
        sccache_before = sccache_setup_step()
        sccache_after = sccache_stats_step()
    else:
        build_body = f"""{link_setup}
          g++-${{{{ matrix.version }}}} -std=c++${{{{ matrix.std }}}} \\
            -Wall -Wextra -Wpedantic -Werror \\
            -O2 -DNDEBUG \\
            -DENABLE_TEST_APPLICATION \\
            -I./include/fat_p \\
            ${{{{ env.TEST_SRC }}}} -o test_bin $LINK_EXTRA"""
        sccache_before = ""
        sccache_after = ""
    return f"""
jobs:
  # ===========================================================================
  # Linux GCC Builds (C++20/C++23)
  # ===========================================================================
  linux-gcc:

    name: Linux GCC-${{{{ matrix.version }}}} C++${{{{ matrix.std }}}}
    runs-on: ubuntu-24.04
    strategy:
      fail-fast: false
      matrix:
        include:
          - version: 12
            std: 20
          - version: 13
            std: 20
          - version: 14
            std: 23
    steps:
      - uses: actions/checkout@v6
{sccache_before}
      - name: Install GCC
        run: sudo apt-get update && sudo apt-get install -y g++-${{{{ matrix.version }}}}

      - name: Build tests
        run: |
{build_body}
{sccache_after}
      - name: Run tests
        run: ./test_bin"""


def generate_linux_clang_job(aggregate=False):
    if aggregate:
        build_body = aggregate_unix_build(
            "sccache clang++-${{ matrix.version }} -std=c++${{ matrix.std }}",
            extra_flags="-Wno-gnu-zero-variadic-macro-arguments \\",
        )
        sccache_before = sccache_setup_step()
        sccache_after = sccache_stats_step()
    else:
        build_body = """          clang++-${{ matrix.version }} -std=c++${{ matrix.std }} \\
            -Wall -Wextra -Wpedantic -Werror \\
            -Wno-gnu-zero-variadic-macro-arguments \\
            -O2 -DNDEBUG \\
            -DENABLE_TEST_APPLICATION \\
            -I./include/fat_p \\
            ${{ env.TEST_SRC }} -o test_bin"""
        sccache_before = ""
        sccache_after = ""
    return """
  # ===========================================================================
  # Linux Clang Builds (C++20/C++23)
  # ===========================================================================
  linux-clang:

    name: Linux Clang-${{ matrix.version }} C++${{ matrix.std }}
    runs-on: ubuntu-22.04
    strategy:
      fail-fast: false
      matrix:
        include:
          - version: 16
            std: 20
          - version: 17
            std: 23
    steps:
      - uses: actions/checkout@v6
__SCCACHE_BEFORE__
      - name: Install Clang
        run: |
          wget https://apt.llvm.org/llvm.sh
          chmod +x llvm.sh
          sudo ./llvm.sh ${{ matrix.version }}

      - name: Build tests
        run: |
__BUILD_BODY__
__SCCACHE_AFTER__
      - name: Run tests
        run: ./test_bin""".replace("__BUILD_BODY__", build_body).replace("__SCCACHE_BEFORE__", sccache_before).replace("__SCCACHE_AFTER__", sccache_after)


def generate_windows_msvc_job(test_src, aggregate=False):
    bs_test = backslash_path(test_src)
    if aggregate:
        build_step = f"""{sccache_setup_step()}
      - name: Build tests
        shell: pwsh
        run: |
          python tools/collect_fatp_test_sources.py --msvc | Out-File -Encoding ascii sources.rsp
          cmd /c "sccache cl ${{{{ matrix.flag }}}} /W4 /WX /wd4324 /wd4127 /EHsc /permissive- /Zc:preprocessor /O2 /DNDEBUG /I.\\include\\fat_p @sources.rsp /Fe:test_bin.exe /link advapi32.lib"
{sccache_stats_step()}"""
    else:
        build_step = f"""      - name: Build tests
        shell: cmd
        run: cl ${{{{ matrix.flag }}}} /W4 /WX /wd4324 /wd4127 /EHsc /permissive- /Zc:preprocessor /O2 /DNDEBUG /DENABLE_TEST_APPLICATION /I.\\include\\fat_p {bs_test} /Fe:test_bin.exe /link advapi32.lib"""
    return f"""
  # ===========================================================================
  # Windows MSVC Builds (C++20/C++23)
  # ===========================================================================
  windows-msvc:

    name: Windows MSVC C++${{{{ matrix.std }}}}
    runs-on: windows-latest
    strategy:
      fail-fast: false
      matrix:
        include:
          - std: 20
            flag: "/std:c++20"
          - std: 23
            flag: "/std:c++latest"
    steps:
      - uses: actions/checkout@v6

      - name: Setup MSVC
        uses: TheMrMilchmann/setup-msvc-dev@v4
        with:
          arch: x64

{build_step}

      - name: Run tests
        run: .\\test_bin.exe"""


def generate_sanitizers(aggregate=False, multiple_sources=False):
    compiler = "sccache g++-13 -std=c++20" if aggregate else "g++-13 -std=c++20"
    if multiple_sources and not aggregate:
        build_prefix = """          index=0
          for src in ${{ env.SANITIZER_TEST_SRCS }}; do
            bin="test_bin_${index}"
            g++-13 -std=c++20 -Wall -Wextra -g -O1 \\
              {flags} \\
              -DENABLE_TEST_APPLICATION \\
              -I./include/fat_p \\
              "${src}" -o "${bin}"
            index=$((index + 1))
          done"""
        run_command = """        run: |
          index=0
          for src in ${{ env.SANITIZER_TEST_SRCS }}; do
            ./"test_bin_${index}"
            index=$((index + 1))
          done"""
    else:
        build_prefix = (
        f"""          mapfile -t TEST_SRCS < <(python tools/collect_fatp_test_sources.py)
          {compiler} -Wall -Wextra -g -O1 \\
            {{flags}} \\
            -I./include/fat_p \\
            "${{TEST_SRCS[@]}}" -o test_bin"""
        if aggregate
        else """          g++-13 -std=c++20 -Wall -Wextra -g -O1 \\
            {flags} \\
            -DENABLE_TEST_APPLICATION \\
            -I./include/fat_p \\
            ${{ env.TEST_SRC }} -o test_bin"""
        )
        run_command = "        run: ./test_bin"
    sccache_before = sccache_setup_step() if aggregate else ""
    sccache_after = sccache_stats_step() if aggregate else ""
    return f"""
  # ===========================================================================
  # Sanitizers (C++20)
  # ===========================================================================
  sanitizer-asan:

    name: AddressSanitizer
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v6
{sccache_before}
      - name: Build with ASan
        run: |
{build_prefix.replace("{flags}", "-fsanitize=address -fno-omit-frame-pointer")}
{sccache_after}
      - name: Run with ASan
        env:
          ASAN_OPTIONS: detect_leaks=1:abort_on_error=1
{run_command}

  sanitizer-ubsan:

    name: UndefinedBehaviorSanitizer
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v6
{sccache_before}
      - name: Build with UBSan
        run: |
{build_prefix.replace("{flags}", "-fsanitize=undefined -fno-omit-frame-pointer")}
{sccache_after}
      - name: Run with UBSan
        env:
          UBSAN_OPTIONS: print_stacktrace=1:halt_on_error=1
{run_command}

  sanitizer-tsan:

    name: ThreadSanitizer
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v6
{sccache_before}
      - name: Build with TSan
        run: |
{build_prefix.replace("{flags}", "-fsanitize=thread -fno-omit-frame-pointer")}
{sccache_after}
      - name: Run with TSan
        env:
          TSAN_OPTIONS: halt_on_error=1
{run_command}"""


def generate_header_check(component, header, additional_headers=None):
    extra_checks = ""
    if additional_headers:
        commands = []
        for extra_header in additional_headers:
            commands.append(
                f"""          echo '#include "{extra_header}"' > test_extra_include.cpp
          echo 'int main() {{ return 0; }}' >> test_extra_include.cpp
          g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror \\
            -I./include/fat_p -c test_extra_include.cpp -o /dev/null"""
            )
        extra_checks = "\n\n" + "\n".join(commands)
    return f"""
  # ===========================================================================
  # Header Self-Containment (C++20)
  # ===========================================================================
  header-check:

    name: Header Self-Containment
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v6

      - name: Test header compiles standalone
        run: |
          echo '#include "${{{{ env.HEADER }}}}"' > test_include.cpp
          echo 'int main() {{{{ return 0; }}}}' >> test_include.cpp

          g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror \\
            -I./include/fat_p \\
            -c test_include.cpp -o /dev/null
{extra_checks}

          echo "Header is self-contained"

      - name: Test include order independence
        run: |
          cat > test1.cpp << 'EOF'
          #include "{header}"
          #include <vector>
          #include <algorithm>
          int main() {{{{ return 0; }}}}
          EOF

          cat > test2.cpp << 'EOF'
          #include <algorithm>
          #include <vector>
          #include "{header}"
          int main() {{{{ return 0; }}}}
          EOF

          g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror -I./include/fat_p test1.cpp -o /dev/null
          g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror -I./include/fat_p test2.cpp -o /dev/null

          echo "Include order independent\""""


def generate_strict_warnings(aggregate=False):
    if aggregate:
        build_body = """          mapfile -t TEST_SRCS < <(python tools/collect_fatp_test_sources.py)
          sccache g++-13 -std=c++20 \\
              -Wall -Wextra -Wpedantic \\
              -Wconversion -Wsign-conversion \\
              -Wshadow -Wformat=2 \\
              -Werror \\
              -I./include/fat_p \\
              -o test_strict "${TEST_SRCS[@]}\""""
        sccache_before = sccache_setup_step()
        sccache_after = sccache_stats_step()
    else:
        build_body = """          g++-13 -std=c++20 \\
              -Wall -Wextra -Wpedantic \\
              -Wconversion -Wsign-conversion \\
              -Wshadow -Wformat=2 \\
              -Werror \\
              -DENABLE_TEST_APPLICATION -I./include/fat_p \\
              -o test_strict ${{ env.TEST_SRC }}"""
        sccache_before = ""
        sccache_after = ""
    echo_line = '          echo "No warnings"'
    return f"""
  # ===========================================================================
  # Strict Warnings (C++20)
  # ===========================================================================
  strict-warnings:

    name: Strict Warnings
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v6
{sccache_before}
      - name: Compile with strict warnings
        run: |
{build_body}
{echo_line}
{sccache_after}"""


# Benchmark execution is intentionally generated in standalone manual workflows,
# never in component CI workflows.



def generate_ci_gate(include_msvc=True, include_debug=False):
    debug_needs = ""
    debug_checks = ""
    if include_debug:
        debug_jobs = ["debug-linux-gcc", "debug-linux-clang"]
        if include_msvc:
            debug_jobs.append("debug-windows-msvc")
        debug_needs = ", " + ", ".join(debug_jobs)
        debug_checks = "\n" + "\n".join(
            f'          if [[ "${{{{ needs.{job}.result }}}}" != "success" ]]; then exit 1; fi'
            for job in debug_jobs
        )

    if include_msvc:
        gate = """
  # ===========================================================================
  # CI Gate
  # ===========================================================================
  ci-success:
    name: CI Success
    needs: [linux-gcc, linux-clang, windows-msvc, sanitizer-asan, sanitizer-ubsan, sanitizer-tsan, header-check, strict-warnings__DEBUG_NEEDS__]
    runs-on: ubuntu-latest
    if: always()
    steps:
      - name: Check results
        run: |
          if [[ "${{ needs.linux-gcc.result }}" != "success" ]]; then exit 1; fi
          if [[ "${{ needs.linux-clang.result }}" != "success" ]]; then exit 1; fi
          if [[ "${{ needs.windows-msvc.result }}" != "success" ]]; then exit 1; fi
          if [[ "${{ needs.sanitizer-asan.result }}" != "success" ]]; then exit 1; fi
          if [[ "${{ needs.sanitizer-ubsan.result }}" != "success" ]]; then exit 1; fi
          if [[ "${{ needs.sanitizer-tsan.result }}" != "success" ]]; then exit 1; fi
          if [[ "${{ needs.header-check.result }}" != "success" ]]; then exit 1; fi
          if [[ "${{ needs.strict-warnings.result }}" != "success" ]]; then exit 1; fi__DEBUG_CHECKS__
          echo "All checks passed"
"""
        return gate.replace("__DEBUG_NEEDS__", debug_needs).replace("__DEBUG_CHECKS__", debug_checks)
    else:
        gate = """
  # ===========================================================================
  # CI Gate
  # ===========================================================================
  ci-success:
    name: CI Success
    needs: [linux-gcc, linux-clang, sanitizer-asan, sanitizer-ubsan, sanitizer-tsan, header-check, strict-warnings__DEBUG_NEEDS__]
    runs-on: ubuntu-latest
    if: always()
    steps:
      - name: Check results
        run: |
          if [[ "${{ needs.linux-gcc.result }}" != "success" ]]; then exit 1; fi
          if [[ "${{ needs.linux-clang.result }}" != "success" ]]; then exit 1; fi
          if [[ "${{ needs.sanitizer-asan.result }}" != "success" ]]; then exit 1; fi
          if [[ "${{ needs.sanitizer-ubsan.result }}" != "success" ]]; then exit 1; fi
          if [[ "${{ needs.sanitizer-tsan.result }}" != "success" ]]; then exit 1; fi
          if [[ "${{ needs.header-check.result }}" != "success" ]]; then exit 1; fi
          if [[ "${{ needs.strict-warnings.result }}" != "success" ]]; then exit 1; fi__DEBUG_CHECKS__
          echo "All checks passed"
"""
        return gate.replace("__DEBUG_NEEDS__", debug_needs).replace("__DEBUG_CHECKS__", debug_checks)


def generate_workflow(filename, component, header, test_src, bench_src, include_msvc=True):
    """Generate a complete workflow file."""
    aggregate = filename == FATP_AGGREGATE_WORKFLOW
    parts = []

    # Header comment
    parts.append(generate_header_block(filename, component, header, test_src, bench_src))

    # Workflow name
    parts.append(f"\nname: {component} CI")

    # Trigger
    if aggregate:
        parts.append(generate_aggregate_trigger_block(filename))
    else:
        parts.append(generate_trigger_block(filename, header, test_src, bench_src))

    # Env block
    sanitizer_sources = SANITIZER_TEST_SOURCES.get(filename)
    parts.append(generate_env_block(header, test_src, sanitizer_sources))

    # Jobs
    gcc_extra = COMPONENT_GCC_CPP23_EXTRA_LIBS.get(component, "")
    parts.append(generate_linux_gcc_job(gcc_extra, aggregate=aggregate))
    parts.append(generate_linux_clang_job(aggregate=aggregate))

    if include_msvc:
        parts.append(generate_windows_msvc_job(test_src, aggregate=aggregate))

    include_debug = filename in DEBUG_WORKFLOWS
    if include_debug:
        parts.append(generate_debug_jobs(test_src, include_msvc=include_msvc))

    parts.append(generate_sanitizers(aggregate=aggregate, multiple_sources=bool(sanitizer_sources)))
    parts.append(generate_header_check(component, header, ADDITIONAL_HEADER_CHECKS.get(filename)))
    parts.append(generate_strict_warnings(aggregate=aggregate))

    parts.append(generate_ci_gate(include_msvc, include_debug=include_debug))

    return "\n".join(parts)


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    count = 0

    # Standard components (with MSVC)
    for filename, component, header, test_src, bench_src in STANDARD_COMPONENTS:
        content = generate_workflow(filename, component, header, test_src, bench_src, include_msvc=True)
        path = os.path.join(OUTPUT_DIR, filename)
        with open(path, "w", newline="\n") as f:
            f.write(content)
        count += 1
        print(f"  Generated: {filename}")

    # Linux-only components (no MSVC)
    for filename, component, header, test_src, bench_src in LINUX_ONLY_COMPONENTS:
        content = generate_workflow(filename, component, header, test_src, bench_src, include_msvc=False)
        path = os.path.join(OUTPUT_DIR, filename)
        with open(path, "w", newline="\n") as f:
            f.write(content)
        count += 1
        print(f"  Generated: {filename} (Linux-only)")

    print(f"\nGenerated {count} workflow files in {OUTPUT_DIR}")


if __name__ == "__main__":
    main()
