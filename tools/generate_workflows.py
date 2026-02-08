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

# =============================================================================
# Component definitions
# =============================================================================
# Each tuple: (workflow_filename, component_name, header, test_src, bench_src_or_None)
# For Linux-only components, we use a separate list.

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

    # --- TypeTraits ---
    ("fatp-type-traits.yml", "FatPTypeTraits", "FatPTypeTraits.h",
     "components/TypeTraits/tests/test_FatPTypeTraits.cpp", None),

    ("type-traits.yml", "TypeTraits", "TypeTraits.h",
     "components/TypeTraits/tests/test_TypeTraits.cpp", None),

    # --- Tensor (6 test files) ---
    ("tensor.yml", "Tensor", "Tensor.h",
     "components/Tensor/tests/test_Tensor.cpp", None),

    ("tensor-comparison.yml", "TensorComparison", "EqualityTensor.h",
     "components/Tensor/tests/test_TensorComparison.cpp", None),

    ("tensor-einsum.yml", "TensorEinsum", "TensorEinsum.h",
     "components/Tensor/tests/test_TensorEinsum.cpp", None),

    ("tensor-math.yml", "TensorMath", "TensorMath.h",
     "components/Tensor/tests/test_TensorMath.cpp", None),

    ("tensor-serializer.yml", "TensorSerializer", "TensorSerializer.h",
     "components/Tensor/tests/test_TensorSerializer.cpp", None),

    ("tensor-storage.yml", "TensorStorage", "TensorStorage.h",
     "components/Tensor/tests/test_TensorStorage.cpp", None),

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

    ("pipe-operator.yml", "PipeOperator", "PipeOperator.h",
     "components/PipeOperator/tests/test_PipeOperator.cpp", None),

    ("rate-limiter.yml", "RateLimiter", "RateLimiter.h",
     "components/RateLimiter/tests/test_RateLimiter.cpp", None),

    ("reflection.yml", "Reflection", "Reflection.h",
     "components/Reflection/tests/test_Reflection.cpp", None),

    ("signal.yml", "Signal", "Signal.h",
     "components/Signal/tests/test_Signal.cpp", None),

    ("simd-vector.yml", "SimdVector", "SimdVector.h",
     "components/SimdVector/tests/test_SimdVector.cpp", None),

    ("sorted-container.yml", "SortedContainer", "SortedContainer.h",
     "components/SortedContainer/tests/test_SortedContainer.cpp", None),

    ("string-pool.yml", "StringPool", "StringPool.h",
     "components/StringPool/tests/test_StringPool.cpp", None),

    ("thread-pool.yml", "ThreadPool", "ThreadPool.h",
     "components/ThreadPool/tests/test_ThreadPool.cpp", None),

    ("value-guard.yml", "ValueGuard", "ValueGuard.h",
     "components/ValueGuard/tests/test_ValueGuard.cpp", None),

    ("view-lifetime-tracking.yml", "ViewLifetimeTracking", "ViewLifetimeTracking.h",
     "components/ViewLifetimeTracking/tests/test_ViewLifetimeTracking.cpp", None),

    # --- Single-test components (previously missing, Gap 2) ---
    ("async-operations.yml", "AsyncOperations", "AsyncOperations.h",
     "components/AsyncOperations/tests/test_AsyncOperations.cpp", None),

    ("atomic-shared-ptr.yml", "AtomicSharedPtr", "AtomicSharedPtr.h",
     "components/AtomicSharedPtr/tests/test_AtomicSharedPtr.cpp", None),

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

    ("policy-queue.yml", "PolicyQueue", "PolicyQueue.h",
     "components/PolicyQueue/tests/test_PolicyQueue.cpp", None),

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
    lines.append(f"# =============================================================================")
    return "\n".join(lines)


def generate_trigger_block(has_benchmarks, filename, header, test_src, bench_src):
    """Generate the on: trigger block with push/pull_request path triggers."""
    # Build path list: header, test file, benchmark file (if any), workflow itself
    paths = [
        f"include/fat_p/{header}",
        test_src,
    ]
    if bench_src:
        paths.append(bench_src)
    paths.append(f".github/workflows/{filename}")

    path_yaml = "\n".join(f"      - '{p}'" for p in paths)

    if has_benchmarks:
        return f"""
on:
  workflow_dispatch:
    inputs:
      run_benchmarks:
        description: 'Run benchmarks'
        required: false
        default: 'false'
        type: boolean
  push:
    paths:
{path_yaml}
  pull_request:
    paths:
{path_yaml}"""
    else:
        return f"""
on:
  workflow_dispatch:
  push:
    paths:
{path_yaml}
  pull_request:
    paths:
{path_yaml}"""


def generate_env_block(header, test_src, bench_src):
    """Generate the env: block."""
    lines = [
        "",
        "env:",
        f"  HEADER: {header}",
        f"  TEST_SRC: {test_src}",
    ]
    if bench_src:
        lines.append(f"  BENCH_SRC: {bench_src}")
    return "\n".join(lines)


def generate_linux_gcc_job():
    return """
jobs:
  # ===========================================================================
  # Linux GCC Builds (C++20/C++23)
  # ===========================================================================
  linux-gcc:

    name: Linux GCC-${{ matrix.version }} C++${{ matrix.std }}
    runs-on: ubuntu-24.04
    strategy:
      fail-fast: false
      matrix:
        include:
          - version: 13
            std: 20
          - version: 14
            std: 23
    steps:
      - uses: actions/checkout@v4

      - name: Install GCC
        run: sudo apt-get update && sudo apt-get install -y g++-${{ matrix.version }}

      - name: Build tests
        run: |
          g++-${{ matrix.version }} -std=c++${{ matrix.std }} \\
            -Wall -Wextra -Wpedantic -Werror \\
            -O2 -DNDEBUG \\
            -DENABLE_TEST_APPLICATION \\
            -I./include/fat_p \\
            ${{ env.TEST_SRC }} -o test_bin

      - name: Run tests
        run: ./test_bin"""


def generate_linux_clang_job():
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
      - uses: actions/checkout@v4

      - name: Install Clang
        run: |
          wget https://apt.llvm.org/llvm.sh
          chmod +x llvm.sh
          sudo ./llvm.sh ${{ matrix.version }}

      - name: Build tests
        run: |
          clang++-${{ matrix.version }} -std=c++${{ matrix.std }} \\
            -Wall -Wextra -Wpedantic -Werror \\
            -Wno-gnu-zero-variadic-macro-arguments \\
            -O2 -DNDEBUG \\
            -DENABLE_TEST_APPLICATION \\
            -I./include/fat_p \\
            ${{ env.TEST_SRC }} -o test_bin

      - name: Run tests
        run: ./test_bin"""


def generate_windows_msvc_job(test_src):
    bs_test = backslash_path(test_src)
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
      - uses: actions/checkout@v4

      - name: Setup MSVC
        uses: ilammy/msvc-dev-cmd@v1

      - name: Build tests
        shell: cmd
        run: cl ${{{{ matrix.flag }}}} /W4 /WX /wd4324 /wd4127 /EHsc /permissive- /Zc:preprocessor /O2 /DNDEBUG /DENABLE_TEST_APPLICATION /I.\\include\\fat_p {bs_test} /Fe:test_bin.exe /link advapi32.lib

      - name: Run tests
        run: .\\test_bin.exe"""


def generate_sanitizers():
    return """
  # ===========================================================================
  # Sanitizers (C++20)
  # ===========================================================================
  sanitizer-asan:

    name: AddressSanitizer
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4

      - name: Build with ASan
        run: |
          g++-13 -std=c++20 -Wall -Wextra -g -O1 \\
            -fsanitize=address -fno-omit-frame-pointer \\
            -DENABLE_TEST_APPLICATION \\
            -I./include/fat_p \\
            ${{ env.TEST_SRC }} -o test_bin

      - name: Run with ASan
        env:
          ASAN_OPTIONS: detect_leaks=1:abort_on_error=1
        run: ./test_bin

  sanitizer-ubsan:

    name: UndefinedBehaviorSanitizer
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4

      - name: Build with UBSan
        run: |
          g++-13 -std=c++20 -Wall -Wextra -g -O1 \\
            -fsanitize=undefined -fno-omit-frame-pointer \\
            -DENABLE_TEST_APPLICATION \\
            -I./include/fat_p \\
            ${{ env.TEST_SRC }} -o test_bin

      - name: Run with UBSan
        env:
          UBSAN_OPTIONS: print_stacktrace=1:halt_on_error=1
        run: ./test_bin

  sanitizer-tsan:

    name: ThreadSanitizer
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4

      - name: Build with TSan
        run: |
          g++-13 -std=c++20 -Wall -Wextra -g -O1 \\
            -fsanitize=thread -fno-omit-frame-pointer \\
            -DENABLE_TEST_APPLICATION \\
            -I./include/fat_p \\
            ${{ env.TEST_SRC }} -o test_bin

      - name: Run with TSan
        env:
          TSAN_OPTIONS: halt_on_error=1
        run: ./test_bin"""


def generate_header_check(component, header):
    return f"""
  # ===========================================================================
  # Header Self-Containment (C++20)
  # ===========================================================================
  header-check:

    name: Header Self-Containment
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4

      - name: Test header compiles standalone
        run: |
          echo '#include "${{{{ env.HEADER }}}}"' > test_include.cpp
          echo 'int main() {{{{ return 0; }}}}' >> test_include.cpp

          g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror \\
            -I./include/fat_p \\
            -c test_include.cpp -o /dev/null

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


def generate_strict_warnings():
    return """
  # ===========================================================================
  # Strict Warnings (C++20)
  # ===========================================================================
  strict-warnings:

    name: Strict Warnings
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4

      - name: Compile with strict warnings
        run: |
          g++-13 -std=c++20 \\
              -Wall -Wextra -Wpedantic \\
              -Wconversion -Wsign-conversion \\
              -Wshadow -Wformat=2 \\
              -Werror \\
              -DENABLE_TEST_APPLICATION -I./include/fat_p \\
              -o test_strict ${{ env.TEST_SRC }}
          echo "No warnings\""""


def generate_benchmark_jobs(component, bench_src):
    bs_bench = backslash_path(bench_src)
    return f"""
  # ===========================================================================
  # Benchmarks - Linux GCC (Manual)
  # ===========================================================================
  benchmarks-gcc:
    name: Benchmark GCC-${{{{ matrix.version }}}} C++${{{{ matrix.std }}}}
    runs-on: ubuntu-latest
    if: ${{{{ inputs.run_benchmarks }}}}
    strategy:
      fail-fast: false
      matrix:
        include:
          - version: 13
            std: 20
          - version: 14
            std: 23

    steps:
      - uses: actions/checkout@v4

      - name: Install GCC
        run: sudo apt-get update && sudo apt-get install -y g++-${{{{ matrix.version }}}}

      - name: Check AVX2 support
        run: |
          if grep -q avx2 /proc/cpuinfo; then
            echo "AVX2 supported"
          else
            echo "AVX2 not supported on this runner"
          fi

      - name: Build benchmark
        run: |
          g++-${{{{ matrix.version }}}} -std=c++${{{{ matrix.std }}}} \\
            -O3 -DNDEBUG -mavx2 -mfma \\
            -I./include/fat_p \\
            ${{{{ env.BENCH_SRC }}}} -o bench_bin -pthread

      - name: Run benchmarks
        env:
          FATP_BENCH_WARMUP_RUNS: 3
          FATP_BENCH_BATCHES: 20
          FATP_BENCH_NO_STABILIZE: 1
          FATP_BENCH_OUTPUT_CSV: results_gcc${{{{ matrix.version }}}}_cpp${{{{ matrix.std }}}}.csv
        run: ./bench_bin 2>&1 | tee benchmark_gcc${{{{ matrix.version }}}}_cpp${{{{ matrix.std }}}}.log

      - name: Upload results
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-gcc${{{{ matrix.version }}}}-cpp${{{{ matrix.std }}}}
          path: |
            results_gcc${{{{ matrix.version }}}}_cpp${{{{ matrix.std }}}}.csv
            benchmark_gcc${{{{ matrix.version }}}}_cpp${{{{ matrix.std }}}}.log

  # ===========================================================================
  # Benchmarks - Linux Clang (Manual)
  # ===========================================================================
  benchmarks-clang:
    name: Benchmark Clang-${{{{ matrix.version }}}} C++${{{{ matrix.std }}}}
    runs-on: ubuntu-22.04
    if: ${{{{ inputs.run_benchmarks }}}}
    strategy:
      fail-fast: false
      matrix:
        include:
          - version: 16
            std: 20
          - version: 17
            std: 23

    steps:
      - uses: actions/checkout@v4

      - name: Install Clang
        run: |
          wget https://apt.llvm.org/llvm.sh
          chmod +x llvm.sh
          sudo ./llvm.sh ${{{{ matrix.version }}}}

      - name: Check AVX2 support
        run: |
          if grep -q avx2 /proc/cpuinfo; then
            echo "AVX2 supported"
          else
            echo "AVX2 not supported on this runner"
          fi

      - name: Build benchmark
        run: |
          clang++-${{{{ matrix.version }}}} -std=c++${{{{ matrix.std }}}} \\
            -O3 -DNDEBUG -mavx2 -mfma \\
            -I./include/fat_p \\
            ${{{{ env.BENCH_SRC }}}} -o bench_bin -pthread

      - name: Run benchmarks
        env:
          FATP_BENCH_WARMUP_RUNS: 3
          FATP_BENCH_BATCHES: 20
          FATP_BENCH_NO_STABILIZE: 1
          FATP_BENCH_OUTPUT_CSV: results_clang${{{{ matrix.version }}}}_cpp${{{{ matrix.std }}}}.csv
        run: ./bench_bin 2>&1 | tee benchmark_clang${{{{ matrix.version }}}}_cpp${{{{ matrix.std }}}}.log

      - name: Upload results
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-clang${{{{ matrix.version }}}}-cpp${{{{ matrix.std }}}}
          path: |
            results_clang${{{{ matrix.version }}}}_cpp${{{{ matrix.std }}}}.csv
            benchmark_clang${{{{ matrix.version }}}}_cpp${{{{ matrix.std }}}}.log

  # ===========================================================================
  # Benchmarks - Windows MSVC (Manual)
  # ===========================================================================
  benchmarks-msvc:
    name: Benchmark MSVC C++${{{{ matrix.std }}}}
    runs-on: windows-latest
    if: ${{{{ inputs.run_benchmarks }}}}
    strategy:
      fail-fast: false
      matrix:
        include:
          - std: 20
            flag: "/std:c++20"
          - std: 23
            flag: "/std:c++latest"

    steps:
      - uses: actions/checkout@v4

      - name: Setup MSVC
        uses: ilammy/msvc-dev-cmd@v1
        with:
          arch: x64

      - name: Build benchmark
        shell: cmd
        run: cl /nologo ${{{{ matrix.flag }}}} /W4 /wd4324 /wd4127 /O2 /DNDEBUG /arch:AVX2 /EHsc /permissive- /Zc:preprocessor /I.\\include\\fat_p {bs_bench} /Fe:bench_bin.exe /link advapi32.lib

      - name: Run benchmarks
        env:
          FATP_BENCH_WARMUP_RUNS: 3
          FATP_BENCH_BATCHES: 20
          FATP_BENCH_NO_STABILIZE: "1"
          FATP_BENCH_OUTPUT_CSV: results_msvc_cpp${{{{ matrix.std }}}}.csv
        run: |
          .\\bench_bin.exe 2>&1 | Tee-Object -FilePath benchmark_msvc_cpp${{{{ matrix.std }}}}.log

      - name: Upload results
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-msvc-cpp${{{{ matrix.std }}}}
          path: |
            results_msvc_cpp${{{{ matrix.std }}}}.csv
            benchmark_msvc_cpp${{{{ matrix.std }}}}.log

  # ===========================================================================
  # Benchmark Summary
  # ===========================================================================
  benchmark-summary:
    name: Benchmark Summary
    needs: [benchmarks-gcc, benchmarks-clang, benchmarks-msvc]
    runs-on: ubuntu-latest
    if: ${{{{ always() && inputs.run_benchmarks }}}}
    steps:
      - name: Download all benchmark artifacts
        uses: actions/download-artifact@v4
        with:
          pattern: benchmark-*
          path: all-benchmarks
          merge-multiple: false

      - name: Aggregate results
        run: |
          echo "# Benchmark Summary - {component}" > summary.md
          echo "" >> summary.md
          echo "| Compiler | Status |" >> summary.md
          echo "|----------|--------|" >> summary.md

          for dir in all-benchmarks/benchmark-*/; do
            name=$(basename "$dir")
            if ls "$dir"/*.csv 1>/dev/null 2>&1; then
              echo "| $name | [PASS] |" >> summary.md
            else
              echo "| $name | [FAIL] |" >> summary.md
            fi
          done

          echo "" >> summary.md
          echo "## Individual Results" >> summary.md

          for dir in all-benchmarks/benchmark-*/; do
            name=$(basename "$dir")
            echo "" >> summary.md
            echo "### $name" >> summary.md
            if [ -f "$dir"/*.log ]; then
              echo '```' >> summary.md
              tail -50 "$dir"/*.log >> summary.md
              echo '```' >> summary.md
            fi
          done

          cat summary.md

      - name: Upload combined results
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-summary
          path: |
            all-benchmarks/
            summary.md"""


def generate_ci_gate(include_msvc=True):
    if include_msvc:
        return """
  # ===========================================================================
  # CI Gate
  # ===========================================================================
  ci-success:
    name: CI Success
    needs: [linux-gcc, linux-clang, windows-msvc, sanitizer-asan, sanitizer-ubsan, sanitizer-tsan, header-check, strict-warnings]
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
          if [[ "${{ needs.strict-warnings.result }}" != "success" ]]; then exit 1; fi
          echo "All checks passed"
"""
    else:
        return """
  # ===========================================================================
  # CI Gate
  # ===========================================================================
  ci-success:
    name: CI Success
    needs: [linux-gcc, linux-clang, sanitizer-asan, sanitizer-ubsan, sanitizer-tsan, header-check, strict-warnings]
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
          if [[ "${{ needs.strict-warnings.result }}" != "success" ]]; then exit 1; fi
          echo "All checks passed"
"""


def generate_workflow(filename, component, header, test_src, bench_src, include_msvc=True):
    """Generate a complete workflow file."""
    parts = []

    # Header comment
    parts.append(generate_header_block(filename, component, header, test_src, bench_src))

    # Workflow name
    parts.append(f"\nname: {component} CI")

    # Trigger
    parts.append(generate_trigger_block(bench_src is not None, filename, header, test_src, bench_src))

    # Env block
    parts.append(generate_env_block(header, test_src, bench_src))

    # Jobs
    parts.append(generate_linux_gcc_job())
    parts.append(generate_linux_clang_job())

    if include_msvc:
        parts.append(generate_windows_msvc_job(test_src))

    parts.append(generate_sanitizers())
    parts.append(generate_header_check(component, header))
    parts.append(generate_strict_warnings())

    if bench_src:
        parts.append(generate_benchmark_jobs(component, bench_src))

    parts.append(generate_ci_gate(include_msvc))

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
