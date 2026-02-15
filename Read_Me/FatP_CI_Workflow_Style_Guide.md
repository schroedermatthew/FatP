# Fat-P CI Workflow Style Guide

**Status:** Active  
**Applies to:** All `.github/workflows/*.yml` files for Fat-P components  
**Authority:** Subordinate to the *Fat-P Library Development Guidelines*  
**Version:** 3.0 (February 2026)

---

## 1. Purpose

This guide standardizes GitHub Actions CI workflows for Fat-P library components. Consistent workflows ensure:

- Uniform quality gates across all components
- Predictable CI behavior for contributors
- Compliance with Fat-P Systemic Hygiene Policy
- **Enforcement of C++20 and C++23 builds only**

---

## 2. Directory Structure

All Fat-P components follow this layout:

```
include/fat_p/                              # Headers (.h)
components/<ComponentName>/tests/           # Test files (test_*.cpp)
components/<ComponentName>/benchmarks/      # Benchmark files (benchmark_*.cpp)
.github/workflows/                          # CI workflow files (*.yml)
scripts/                                    # Verification scripts
```

**Critical:** Always use these paths in workflows. The old `FAT_P/FAT_P/...` structure is deprecated.

### Path Examples

| Component | Header | Test | Benchmark |
|-----------|--------|------|-----------|
| ObjectPool | `include/fat_p/ObjectPool.h` | `components/ObjectPool/tests/test_ObjectPool.cpp` | `components/ObjectPool/benchmarks/benchmark_ObjectPool.cpp` |
| SmallVector | `include/fat_p/SmallVector.h` | `components/SmallVector/tests/test_SmallVector.cpp` | `components/SmallVector/benchmarks/benchmark_SmallVector.cpp` |
| FastHashMap | `include/fat_p/FastHashMap.h` | `components/FastHashMap/tests/test_FastHashMap.cpp` | `components/FastHashMap/benchmarks/benchmark_FastHashMap.cpp` |

---

## 3. Workflow Trigger Policy

### 3.1 Component CI Workflows

**Component CI workflows trigger on push, pull_request, and support manual dispatch.**
Each component workflow triggers on pushes and PRs that modify its own files, plus manual dispatch for on-demand runs. Component CI workflows do **not** contain benchmark jobs.

**Required trigger block (component CI workflows):**
```yaml
on:
  workflow_dispatch:
  push:
    paths:
      - 'include/fat_p/<Header>.h'
      - 'components/<Component>/tests/<test_file>.cpp'
      - 'components/<Component>/benchmarks/<bench_file>.cpp'
      - '.github/workflows/<component-name>.yml'
  pull_request:
    paths:
      - 'include/fat_p/<Header>.h'
      - 'components/<Component>/tests/<test_file>.cpp'
      - 'components/<Component>/benchmarks/<bench_file>.cpp'
      - '.github/workflows/<component-name>.yml'
```

Replace `<Header>`, `<Component>`, `<test_file>`, `<bench_file>`, and `<component-name>` with the actual component names. The `push` and `pull_request` paths must be identical.

Benchmark source paths are included so that CI validates benchmark code compiles (via the strict-warnings and header-check jobs that exercise the include graph), even though benchmarks run in the separate benchmark workflow.

Rationale:
- Push triggers with path filtering ensure changes are validated immediately without running unrelated workflows.
- Pull request triggers provide pre-merge validation.
- `workflow_dispatch` remains available for manual reruns.
- The workflow file itself is included in paths so CI changes are self-testing.

### 3.2 Benchmark Workflows

**Benchmark workflows are separate YAML files triggered only by manual dispatch (`workflow_dispatch`).**
They are never embedded in or called by the component CI workflow. This separation exists because benchmarks require third-party competitor libraries (Abseil, Folly, LLVM, tsl, ankerl, Boost) that are expensive to install and irrelevant to correctness testing.

**Required trigger block (benchmark workflows):**
```yaml
on:
  workflow_dispatch:
    inputs:
      batches:
        description: 'Measured batches per benchmark'
        required: false
        default: '20'
        type: string
      target_work:
        description: 'Target ops per batch'
        required: false
        default: '100000'
        type: string
```

**Naming convention:** `<component-name>-benchmarks.yml` (e.g., `fatp-hash-map-benchmarks.yml`).

---

## 4. C++ Standard Policy

**C++20 is the minimum. C++23 is tested for forward compatibility.**

| Standard | Status | Compiler Matrix |
|----------|--------|-----------------|
| C++20 | Primary | GCC-12, GCC-13, Clang-16, MSVC |
| C++23 | Forward compat | GCC-14, Clang-17, MSVC (`/std:c++latest`) |

### Rationale

Fat-P requires C++20 features throughout (concepts, ranges, `std::span`, `constexpr` improvements). C++17 is not compatible with the library and is not tested in CI.

---

## 5. Required Jobs

### 5.1 Component CI Workflow Jobs

Every component CI workflow MUST include these jobs:

| Job | Purpose | Required |
|-----|---------|----------|
| `linux-gcc` | GCC 12/13/14 (C++20/C++23) build + tests | Yes |
| `linux-clang` | Clang 16/17 (C++20/C++23) build + tests | Yes |
| `windows-msvc` | MSVC (C++20/C++23) build + tests | Yes |
| `sanitizer-asan` | AddressSanitizer | Yes |
| `sanitizer-ubsan` | UndefinedBehaviorSanitizer | Yes |
| `sanitizer-tsan` | ThreadSanitizer | Yes (concurrency components) |
| `header-check` | Verify headers compile standalone + include order | Yes |
| `strict-warnings` | Extended warning flags | Yes |
| `ci-success` | Gate job aggregating all results | Yes |

Benchmark jobs do **not** belong in the component CI workflow. See Section 13 for the separate benchmark workflow structure.

### 5.2 Benchmark Workflow Jobs

Every benchmark workflow MUST include these jobs:

| Job | Purpose | Required |
|-----|---------|----------|
| `benchmarks-gcc` | GCC benchmark runs with all competitors | Yes |
| `benchmarks-clang` | Clang benchmark runs with all competitors | Yes |
| `benchmarks-msvc` | MSVC benchmark runs with all competitors | Yes |
| `benchmark-summary` | Aggregate results and write to `$GITHUB_STEP_SUMMARY` | Yes |

---

## 6. Compiler Version Matrix

### 6.1 GCC Versions

| Version | C++ Standard | Runner | Role |
|---------|--------------|--------|------|
| GCC 12 | C++20 | ubuntu-24.04 | Oldest supported |
| GCC 13 | C++20 | ubuntu-24.04 | Primary |
| GCC 14 | C++23 | ubuntu-24.04 | Forward compat |

### 6.2 Clang Versions

| Version | C++ Standard | Runner | Role |
|---------|--------------|--------|------|
| Clang 16 | C++20 | ubuntu-22.04 | Primary |
| Clang 17 | C++23 | ubuntu-22.04 | Forward compat |

### 6.3 MSVC Standards

| Standard | Flag | Role |
|----------|------|------|
| C++20 | `/std:c++20` | Primary |
| C++23 | `/std:c++latest` | Forward compat |

---

## 7. MSVC-Specific Requirements

### 7.1 Required Libraries

MSVC builds **must** link `advapi32.lib` for Windows Registry APIs used by `FatPTest.h` and `FatPBenchmarkRunner.h`:

```yaml
cl ... /Fe:test_bin.exe /link advapi32.lib
```

Without this, you will see linker errors:
```
error LNK2019: unresolved external symbol __imp_RegOpenKeyExA
error LNK2019: unresolved external symbol __imp_RegCloseKey
error LNK2019: unresolved external symbol __imp_RegQueryValueExA
```

### 7.2 Warning Suppressions

| Warning | Flag | Reason |
|---------|------|--------|
| C4324 | `/wd4324` | Structure padded due to alignment specifier (intentional for cache-line alignment in `ConcurrencyPolicies.h`) |

### 7.3 MSVC Build Command Template

```yaml
- name: Build tests
  run: |
    $stdFlag = if (${{ matrix.std }} -eq 23) { "/std:c++latest" } else { "/std:c++${{ matrix.std }}" }
    cl $stdFlag /W4 /WX /wd4324 /EHsc /permissive- /O2 /DNDEBUG /DENABLE_TEST_APPLICATION /I.\include\fat_p components\<Component>\tests\test_<Component>.cpp /Fe:test_bin.exe /link advapi32.lib
```

### 7.4 MSVC Path Format

Windows uses backslashes. In YAML:
```yaml
# Correct
/I.\include\fat_p components\ObjectPool\tests\test_ObjectPool.cpp

# Wrong (will fail)
/I./include/fat_p components/ObjectPool/tests/test_ObjectPool.cpp
```

---

## 8. Linux Build Requirements

### 8.1 GCC Build Command Template

```yaml
- name: Build tests
  run: |
    g++-${{ matrix.version }} -std=c++${{ matrix.std }} \
      -Wall -Wextra -Wpedantic -Werror \
      -O2 -DNDEBUG \
      -DENABLE_TEST_APPLICATION \
      -I./include/fat_p \
      ${{ env.TEST_SRC }} -o test_bin
```

### 8.2 Clang Build Command Template

```yaml
- name: Build tests
  run: |
    clang++-${{ matrix.version }} -std=c++${{ matrix.std }} \
      -Wall -Wextra -Wpedantic -Werror \
      -Wno-gnu-zero-variadic-macro-arguments \
      -O2 -DNDEBUG \
      -DENABLE_TEST_APPLICATION \
      -I./include/fat_p \
      ${{ env.TEST_SRC }} -o test_bin
```

Note: `-Wno-gnu-zero-variadic-macro-arguments` suppresses warnings from `FatPTest.h` macros on Clang.

---

## 9. Sanitizer Jobs

Sanitizers run at C++20 only (sufficient for memory/thread safety coverage).

### 9.1 AddressSanitizer

```yaml
sanitizer-asan:
  name: AddressSanitizer
  runs-on: ubuntu-24.04
  steps:
    - uses: actions/checkout@v4
    - name: Build with ASan
      run: |
        g++-13 -std=c++20 -Wall -Wextra -g -O1 \
          -fsanitize=address -fno-omit-frame-pointer \
          -DENABLE_TEST_APPLICATION \
          -I./include/fat_p \
          ${{ env.TEST_SRC }} -o test_bin
    - name: Run with ASan
      env:
        ASAN_OPTIONS: detect_leaks=1:abort_on_error=1
      run: ./test_bin
```

### 9.2 UndefinedBehaviorSanitizer

```yaml
sanitizer-ubsan:
  name: UndefinedBehaviorSanitizer
  runs-on: ubuntu-24.04
  steps:
    - uses: actions/checkout@v4
    - name: Build with UBSan
      run: |
        g++-13 -std=c++20 -Wall -Wextra -g -O1 \
          -fsanitize=undefined -fno-omit-frame-pointer \
          -DENABLE_TEST_APPLICATION \
          -I./include/fat_p \
          ${{ env.TEST_SRC }} -o test_bin
    - name: Run with UBSan
      env:
        UBSAN_OPTIONS: print_stacktrace=1:halt_on_error=1
      run: ./test_bin
```

### 9.3 ThreadSanitizer

Required for components with concurrency (ObjectPool, LockFreeQueue, ThreadPool, etc.):

```yaml
sanitizer-tsan:
  name: ThreadSanitizer
  runs-on: ubuntu-24.04
  steps:
    - uses: actions/checkout@v4
    - name: Build with TSan
      run: |
        g++-13 -std=c++20 -Wall -Wextra -g -O1 \
          -fsanitize=thread -fno-omit-frame-pointer \
          -DENABLE_TEST_APPLICATION \
          -I./include/fat_p \
          ${{ env.TEST_SRC }} -o test_bin
    - name: Run with TSan
      env:
        TSAN_OPTIONS: halt_on_error=1
      run: ./test_bin
```

---

## 10. Header Hygiene Jobs

### 10.1 Self-Containment Test

Verifies headers compile without requiring other includes first:

```yaml
- name: Test header compiles standalone
  run: |
    echo '#include "${{ env.HEADER }}"' > test_include.cpp
    echo 'int main() { return 0; }' >> test_include.cpp
    
    g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror \
      -I./include/fat_p \
      -c test_include.cpp -o /dev/null
    
    echo "Header is self-contained"
```

### 10.2 Include Order Independence Test

Verifies headers work regardless of include order:

```yaml
- name: Test include order independence
  run: |
    # Component header first
    cat > test1.cpp << 'EOF'
    #include "<Component>.h"
    #include <vector>
    #include <algorithm>
    int main() {
        // Minimal usage
        return 0;
    }
    EOF
    
    # Component header last
    cat > test2.cpp << 'EOF'
    #include <algorithm>
    #include <vector>
    #include "<Component>.h"
    int main() {
        // Minimal usage
        return 0;
    }
    EOF
    
    g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror -I./include/fat_p test1.cpp -o /dev/null
    g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror -I./include/fat_p test2.cpp -o /dev/null
    
    echo "Include order independent"
```

---

## 11. Strict Warnings Job

```yaml
strict-warnings:
  name: Strict Warnings
  runs-on: ubuntu-24.04
  steps:
    - uses: actions/checkout@v4
    - name: Compile with strict warnings
      run: |
        g++-13 -std=c++20 \
            -Wall -Wextra -Wpedantic \
            -Wconversion -Wsign-conversion \
            -Wshadow -Wformat=2 \
            -Werror \
            -DENABLE_TEST_APPLICATION -I./include/fat_p \
            -o test_strict ${{ env.TEST_SRC }}
        echo "No warnings"
```

---

## 12. CI Gate Job

The `ci-success` job aggregates all required job results:

```yaml
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
```

**Important:** Include `sanitizer-tsan` in the needs list for concurrency-related components.

---

## 13. Benchmark Workflows

Benchmarks live in a **separate YAML file** from the component CI workflow. They are triggered only by manual dispatch (`workflow_dispatch`), never by push or pull_request, and are never embedded in or called by the component CI workflow.

**Rationale:** Benchmarks require third-party competitor libraries (Abseil, Folly, LLVM DenseMap, tsl::robin_map, ankerl::unordered_dense, Boost) that are expensive to build and install. Keeping them separate avoids slowing CI runs and cluttering CI workflow files with competitor dependency management.

### 13.1 Benchmark Workflow Naming

| Workflow Type | File Name |
|---------------|-----------|
| Component CI | `<component-name>.yml` |
| Benchmarks | `<component-name>-benchmarks.yml` |

Example: `fatp-hash-map.yml` (CI) + `fatp-hash-map-benchmarks.yml` (benchmarks).

### 13.2 Competitor Libraries

Benchmark workflows should include all relevant competitors for the component being benchmarked. For hash map components, the standard competitor set is:

| Competitor | Type | Linux Install | Windows Install |
|------------|------|---------------|-----------------|
| `std::unordered_map` | Baseline | Always available | Always available |
| `tsl::robin_map` | Header-only | `git clone` | `git clone` |
| `ankerl::unordered_dense` | Header-only | `git clone` | `git clone` |
| `boost::unordered_flat_map` | Header-only | `libboost-dev` | vcpkg |
| `boost::unordered_node_map` | Header-only | `libboost-dev` | vcpkg |
| `absl::flat_hash_map` | Static library | Build from source | vcpkg |
| `absl::node_hash_map` | Static library | Build from source | vcpkg |
| `folly::F14FastMap` | Static library | Build from source | vcpkg |
| `folly::F14NodeMap` | Static library | Build from source | vcpkg |
| `llvm::DenseMap` | Static library | Distro package | vcpkg |

Other components should define their own competitor sets appropriate to the domain.

### 13.3 Third-Party Caching

Third-party competitor libraries MUST be cached using `actions/cache@v4`. Building Abseil, Folly, and LLVM from source takes significant time; caching avoids repeating this on every run.

**Caching rules:**

| Rule | Detail |
|------|--------|
| Cache path | `~/thirdparty` (Linux), `thirdparty` (Windows for header-only), vcpkg binary cache (Windows for compiled) |
| Cache key | Must include compiler version to avoid ABI mismatches (e.g., `hashmap-bench-all-gcc14-v2`) |
| Conditional install | Use `if: steps.cache-deps.outputs.cache-hit != 'true'` on all install/build steps |
| Bump key version | Increment version suffix (`-v2`, `-v3`) when changing dependencies or build flags |
| Runtime dependencies | System shared libraries needed at link time (e.g., `-lfmt`, `-lglog`) must be installed unconditionally, not gated on the cache. The cache stores compiled artifacts; the system packages provide the shared libraries for linking. |

**Linux caching pattern (GCC example):**

```yaml
- name: Cache third-party libraries
  id: cache-deps
  uses: actions/cache@v4
  with:
    path: ~/thirdparty
    key: hashmap-bench-all-gcc${{ matrix.version }}-v2

- name: Install header-only competitors
  if: steps.cache-deps.outputs.cache-hit != 'true'
  run: |
    mkdir -p $HOME/thirdparty
    git clone --depth 1 https://github.com/Tessil/robin-map.git \
      $HOME/thirdparty/robin-map
    git clone --depth 1 https://github.com/martinus/unordered_dense.git \
      $HOME/thirdparty/unordered_dense

- name: Build Abseil from source
  if: steps.cache-deps.outputs.cache-hit != 'true'
  run: |
    git clone --depth 1 https://github.com/abseil/abseil-cpp.git \
      $HOME/thirdparty/abseil-src
    cmake -S $HOME/thirdparty/abseil-src \
          -B $HOME/thirdparty/abseil-build \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_CXX_STANDARD=20 \
          -DCMAKE_CXX_COMPILER=g++-${{ matrix.version }} \
          -DABSL_BUILD_TESTING=OFF \
          -DABSL_USE_GOOGLETEST_HEAD=OFF \
          -DCMAKE_INSTALL_PREFIX=$HOME/thirdparty/abseil
    cmake --build $HOME/thirdparty/abseil-build --parallel $(nproc)
    cmake --install $HOME/thirdparty/abseil-build
```

**Windows caching pattern (vcpkg):**

```yaml
- name: Cache header-only libraries
  id: cache-headeronly
  uses: actions/cache@v4
  with:
    path: thirdparty
    key: hashmap-bench-headeronly-${{ runner.os }}-v2

- name: Cache vcpkg packages
  uses: actions/cache@v4
  with:
    path: ${{ github.workspace }}/vcpkg_cache
    key: vcpkg-${{ runner.os }}-hashmap-bench-all-v2
    restore-keys: |
      vcpkg-${{ runner.os }}-hashmap-bench-all-

- name: Install competitors via vcpkg
  shell: cmd
  env:
    VCPKG_DEFAULT_TRIPLET: x64-windows
    VCPKG_DEFAULT_BINARY_CACHE: ${{ github.workspace }}/vcpkg_cache
  run: |
    if not exist "${{ github.workspace }}\vcpkg_cache" mkdir "${{ github.workspace }}\vcpkg_cache"
    vcpkg install abseil:x64-windows boost-unordered:x64-windows folly:x64-windows llvm:x64-windows fmt:x64-windows
```

### 13.4 Benchmark Environment Variables

All benchmarks must support these canonical environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `FATP_BENCH_WARMUP_RUNS` | 3 | Warmup batches (not reported) |
| `FATP_BENCH_BATCHES` | 20 | Measured batches |
| `FATP_BENCH_TARGET_WORK` | 100000 | Target ops per batch |
| `FATP_BENCH_NO_STABILIZE` | 1 | Disable CPU stabilization (CI) |
| `FATP_BENCH_OUTPUT_CSV` | (set) | CSV output path |

All environment variable values MUST be quoted strings in YAML to avoid type coercion issues across platforms.

### 13.5 Benchmark Build Flags

Benchmarks build with C++20 and full optimization:

```yaml
# Linux
g++-${{ matrix.version }} -std=c++20 \
  -O3 -DNDEBUG -march=native \
  -I./include/fat_p \
  ${{ env.BENCH_SRC }} -o bench_bin -pthread

# Windows
cl /nologo /std:c++20 /O2 /DNDEBUG /EHsc /permissive- \
  /Zc:preprocessor /wd4324 \
  /I.\include\fat_p components\<Component>\benchmarks\benchmark_<Component>.cpp \
  /Fe:bench_bin.exe /link advapi32.lib
```

### 13.6 Benchmark Summary Job

Every benchmark workflow MUST include a `benchmark-summary` job that:

1. Depends on all benchmark jobs (`needs: [benchmarks-gcc, benchmarks-clang, benchmarks-msvc]`)
2. Uses `if: always()` so it runs even if some benchmarks fail
3. Downloads all benchmark artifacts using a pattern match
4. Writes a summary table to `$GITHUB_STEP_SUMMARY`
5. Uploads the combined results as a single artifact

**Artifact naming convention:** `bench-<ComponentName>-<compiler>` for individual results, `bench-<ComponentName>-summary` for the combined artifact.

---

## 14. Output Format Requirements

### 14.1 ASCII Only

All CI output must be ASCII-only. No Unicode characters.

| Instead of | Use |
|------------|-----|
| âœ“ | `[PASS]` or `[x]` |
| âœ— | `[FAIL]` or `[ ]` |
| âŒ | `[X]` or `[FAIL]` |
| âš  | `[WARNING]` or `[!]` |

### 14.2 Success Messages

Use simple text without emoji:
```yaml
echo "Header is self-contained"    # Good
echo "âœ“ Header is self-contained"  # Bad (Unicode)
```

---

## 15. Complete Workflow Template

### 15.1 Component CI Workflow Template

```yaml
# =============================================================================
# .github/workflows/<component-name>.yml
# =============================================================================
# CI workflow for <ComponentName> component
#
# Directory structure:
#   Headers:    include/fat_p/<Component>.h
#   Tests:      components/<Component>/tests/test_<Component>.cpp
#   Benchmarks: components/<Component>/benchmarks/benchmark_<Component>.cpp
#
# Benchmarks are run separately via <component-name>-benchmarks.yml
# (manual dispatch only).
# =============================================================================

name: <ComponentName> CI

on:
  workflow_dispatch:
  push:
    paths:
      - 'include/fat_p/<Component>.h'
      - 'components/<Component>/tests/test_<Component>.cpp'
      - 'components/<Component>/benchmarks/benchmark_<Component>.cpp'
      - '.github/workflows/<component-name>.yml'
  pull_request:
    paths:
      - 'include/fat_p/<Component>.h'
      - 'components/<Component>/tests/test_<Component>.cpp'
      - 'components/<Component>/benchmarks/benchmark_<Component>.cpp'
      - '.github/workflows/<component-name>.yml'

env:
  HEADER: <Component>.h
  TEST_SRC: components/<Component>/tests/test_<Component>.cpp
  BENCH_SRC: components/<Component>/benchmarks/benchmark_<Component>.cpp

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
          - version: 12
            std: 20
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
          g++-${{ matrix.version }} -std=c++${{ matrix.std }} \
            -Wall -Wextra -Wpedantic -Werror \
            -O2 -DNDEBUG \
            -DENABLE_TEST_APPLICATION \
            -I./include/fat_p \
            ${{ env.TEST_SRC }} -o test_bin
      - name: Run tests
        run: ./test_bin

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
          clang++-${{ matrix.version }} -std=c++${{ matrix.std }} \
            -Wall -Wextra -Wpedantic -Werror \
            -Wno-gnu-zero-variadic-macro-arguments \
            -O2 -DNDEBUG \
            -DENABLE_TEST_APPLICATION \
            -I./include/fat_p \
            ${{ env.TEST_SRC }} -o test_bin
      - name: Run tests
        run: ./test_bin

  # ===========================================================================
  # Windows MSVC Builds (C++20/C++23)
  # ===========================================================================
  windows-msvc:
    name: Windows MSVC C++${{ matrix.std }}
    runs-on: windows-latest
    strategy:
      fail-fast: false
      matrix:
        std: [20, 23]
    steps:
      - uses: actions/checkout@v4
      - name: Setup MSVC
        uses: ilammy/msvc-dev-cmd@v1
      - name: Build tests
        run: |
          $stdFlag = if (${{ matrix.std }} -eq 23) { "/std:c++latest" } else { "/std:c++${{ matrix.std }}" }
          cl $stdFlag /W4 /WX /wd4324 /EHsc /permissive- /O2 /DNDEBUG /DENABLE_TEST_APPLICATION /I.\include\fat_p components\<Component>\tests\test_<Component>.cpp /Fe:test_bin.exe /link advapi32.lib
      - name: Run tests
        run: .\test_bin.exe

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
          g++-13 -std=c++20 -Wall -Wextra -g -O1 \
            -fsanitize=address -fno-omit-frame-pointer \
            -DENABLE_TEST_APPLICATION \
            -I./include/fat_p \
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
          g++-13 -std=c++20 -Wall -Wextra -g -O1 \
            -fsanitize=undefined -fno-omit-frame-pointer \
            -DENABLE_TEST_APPLICATION \
            -I./include/fat_p \
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
          g++-13 -std=c++20 -Wall -Wextra -g -O1 \
            -fsanitize=thread -fno-omit-frame-pointer \
            -DENABLE_TEST_APPLICATION \
            -I./include/fat_p \
            ${{ env.TEST_SRC }} -o test_bin
      - name: Run with TSan
        env:
          TSAN_OPTIONS: halt_on_error=1
        run: ./test_bin

  # ===========================================================================
  # Header Self-Containment
  # ===========================================================================
  header-check:
    name: Header Self-Containment
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Test header compiles standalone
        run: |
          echo '#include "${{ env.HEADER }}"' > test_include.cpp
          echo 'int main() { return 0; }' >> test_include.cpp
          g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror \
            -I./include/fat_p \
            -c test_include.cpp -o /dev/null
          echo "Header is self-contained"
      - name: Test include order independence
        run: |
          cat > test1.cpp << 'EOF'
          #include "<Component>.h"
          #include <vector>
          #include <algorithm>
          int main() { return 0; }
          EOF
          cat > test2.cpp << 'EOF'
          #include <algorithm>
          #include <vector>
          #include "<Component>.h"
          int main() { return 0; }
          EOF
          g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror -I./include/fat_p test1.cpp -o /dev/null
          g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror -I./include/fat_p test2.cpp -o /dev/null
          echo "Include order independent"

  # ===========================================================================
  # Strict Warnings
  # ===========================================================================
  strict-warnings:
    name: Strict Warnings
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Compile with strict warnings
        run: |
          g++-13 -std=c++20 \
              -Wall -Wextra -Wpedantic \
              -Wconversion -Wsign-conversion \
              -Wshadow -Wformat=2 \
              -Werror \
              -DENABLE_TEST_APPLICATION -I./include/fat_p \
              -o test_strict ${{ env.TEST_SRC }}
          echo "No warnings"

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
```

---

## 16. Checklist for New Workflows

### Component CI Workflow Checklist

Before committing a new component CI workflow:

- [ ] File named `.github/workflows/<component-name>.yml`
- [ ] Header block with directory structure documented
- [ ] Header comment notes benchmarks are in separate workflow
- [ ] All paths use `include/fat_p/` and `components/<Component>/...`
- [ ] `env:` block defines HEADER, TEST_SRC, BENCH_SRC
- [ ] `linux-gcc` job with GCC 12 (C++20), GCC 13 (C++20), and GCC 14 (C++23)
- [ ] `linux-clang` job with Clang 16 (C++20) and Clang 17 (C++23)
- [ ] `windows-msvc` job with C++20 and C++23
- [ ] MSVC uses `/wd4324` to suppress alignment warnings
- [ ] MSVC links `advapi32.lib`
- [ ] MSVC uses backslash paths
- [ ] `sanitizer-asan` job
- [ ] `sanitizer-ubsan` job
- [ ] `sanitizer-tsan` job (for concurrency components)
- [ ] `header-check` job with standalone and include-order tests
- [ ] `strict-warnings` job
- [ ] `ci-success` gate job with all required jobs in `needs`
- [ ] No benchmark jobs in the CI workflow
- [ ] No `run_benchmarks` input in the CI workflow
- [ ] ASCII-only output (no Unicode symbols)
- [ ] Tested locally or validated manually

### Benchmark Workflow Checklist

Before committing a new benchmark workflow:

- [ ] File named `.github/workflows/<component-name>-benchmarks.yml`
- [ ] Trigger is `workflow_dispatch` only (no push, no pull_request, no workflow_call)
- [ ] `inputs.batches` and `inputs.target_work` parameters defined
- [ ] `benchmarks-gcc` job with GCC 12/13/14
- [ ] `benchmarks-clang` job with Clang 16/17
- [ ] `benchmarks-msvc` job
- [ ] All competitors cached with `actions/cache@v4`
- [ ] Cache keys include compiler version (e.g., `gcc14`, `clang17`)
- [ ] Install/build steps gated on `cache-hit != 'true'`
- [ ] Runtime link dependencies installed unconditionally
- [ ] Benchmark env vars quoted (`"3"`, `"1"`, not bare `3`, `1`)
- [ ] `FATP_BENCH_TARGET_WORK` passed from `inputs.target_work`
- [ ] `benchmark-summary` job with `if: always()`
- [ ] Summary written to `$GITHUB_STEP_SUMMARY`
- [ ] Artifact names follow `bench-<ComponentName>-<compiler>` convention
- [ ] ASCII-only output

---

## 17. Common Mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Wrong paths | `file not found` | Use `include/fat_p/` and `components/.../` |
| Missing `advapi32.lib` | `LNK2019: __imp_RegOpenKeyExA` | Add `/link advapi32.lib` |
| Missing `/wd4324` | `C4324: structure was padded` | Add `/wd4324` |
| Forward slashes on Windows | `file not found` | Use backslashes: `.\include\fat_p` |
| Unicode in output | Display issues in logs | Use ASCII: `[PASS]` not `âœ“` |
| Missing TSan in ci-success | Gate passes despite TSan failure | Add `sanitizer-tsan` to `needs` |
| Old `FAT_P/FAT_P/` paths | `file not found` | Update to new structure |
| Adding C++17 jobs | C++17 is not supported | Test C++20 and C++23 only |
| Benchmark jobs in CI workflow | CI takes too long, competitor deps clutter CI | Move to separate `<component>-benchmarks.yml` |
| Unquoted benchmark env vars | Type coercion differs across platforms | Quote all values: `"3"` not `3` |
| Cache key missing compiler version | ABI mismatch in cached libraries | Include compiler version in key: `gcc14`, `clang17` |

---

## 18. Changelog

### v3.1 (February 2026)
- Added GCC 12 (C++20) to compiler matrix as "Oldest supported"
- **Breaking:** Benchmark jobs are now separate YAML files, not embedded in component CI workflows
- Rewrote Section 3: Split into 3.1 (component CI triggers) and 3.2 (benchmark triggers); removed `run_benchmarks` input from CI workflows
- Rewrote Section 5: Split into 5.1 (CI jobs) and 5.2 (benchmark jobs); removed Optional Jobs subsection
- Rewrote Section 13: Comprehensive benchmark workflow documentation including competitor libraries (Section 13.2), third-party caching rules with patterns (Section 13.3), environment variables with quoting requirement (Section 13.4), build flags at C++20 (Section 13.5), and summary job requirements (Section 13.6)
- Updated Section 15: Template removes `run_benchmarks` input, header comment references separate benchmark workflow
- Updated Section 16: Split into Component CI Checklist and Benchmark Workflow Checklist
- Updated Section 17: Added common mistakes for benchmarks-in-CI, unquoted env vars, and cache key versioning
- Updated Sections 4, 5, 6.1, 15, and 16 to include GCC 12
- GCC matrix is now: GCC 12 (C++20), GCC 13 (C++20), GCC 14 (C++23)

### v3.0 (February 2026)
- **Breaking:** Updated directory structure from `FAT_P/FAT_P/...` to `include/fat_p/` and `components/.../`
- **Breaking:** Dropped C++17 support; now C++20/C++23 only
- Added Section 7: MSVC-specific requirements (`/wd4324`, `advapi32.lib`, backslash paths)
- Added Section 14: ASCII-only output requirements
- Added Section 17: Common mistakes reference
- Updated compiler matrix: GCC 13/14, Clang 16/17, MSVC C++20/C++23 (GCC 12 added in v3.1)
- Removed all references to `ci_core17.yml` and C++17 gates

### v2.1 (January 2026)
- Updated layer integrity guidance for `FATP_META.layer`

### v2.0 (January 2026)
- Added project-wide verification workflows
- Added C++17 core compile gate (now removed in v3.0)

### v1.0 (December 2025)
- Initial CI Workflow Style Guide

---

*Fat-P CI Workflow Style Guide v3.1 -- February 2026*
