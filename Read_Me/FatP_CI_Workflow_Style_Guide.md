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
- **Enforcement of C++20/C++23 builds** (C++17 support dropped)

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

**Fat-P component CI workflows MUST trigger on push, pull_request, and support manual dispatch.**  
Each component workflow triggers on pushes and PRs that modify its own files, plus manual dispatch for on-demand runs.

**Required trigger block (component workflows):**
```yaml
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
      - 'include/fat_p/<Header>.h'
      - 'components/<Component>/tests/<test_file>.cpp'
      - 'components/<Component>/benchmarks/<bench_file>.cpp'
      - '.github/workflows/<workflow>.yml'
  pull_request:
    paths:
      - 'include/fat_p/<Header>.h'
      - 'components/<Component>/tests/<test_file>.cpp'
      - 'components/<Component>/benchmarks/<bench_file>.cpp'
      - '.github/workflows/<workflow>.yml'
```

Replace `<Header>`, `<Component>`, `<test_file>`, `<bench_file>`, and `<workflow>` with the actual component names. The `push` and `pull_request` paths must be identical.

Rationale:
- Push triggers with path filtering ensure changes are validated immediately without running unrelated workflows.
- Pull request triggers provide pre-merge validation.
- `workflow_dispatch` remains available for manual reruns and benchmark runs.
- The workflow file itself is included in paths so CI changes are self-testing.

---

## 4. C++ Standard Policy

**C++20 is the minimum. C++23 is tested for forward compatibility. C++17 is not supported.**

| Standard | Status | Compiler Matrix |
|----------|--------|-----------------|
| C++17 | **Dropped** | Not tested |
| C++20 | Primary | GCC-13, Clang-16, MSVC |
| C++23 | Forward compat | GCC-14, Clang-17, MSVC (`/std:c++latest`) |

### Rationale

Fat-P uses C++20 features extensively (concepts, ranges, `std::span`, `constexpr` improvements). C++17 support was dropped to reduce maintenance burden and enable cleaner APIs.

---

## 5. Required Jobs

Every component workflow MUST include these jobs:

| Job | Purpose | Required |
|-----|---------|----------|
| `linux-gcc` | GCC 13/14 (C++20/C++23) build + tests | Yes |
| `linux-clang` | Clang 16/17 (C++20/C++23) build + tests | Yes |
| `windows-msvc` | MSVC (C++20/C++23) build + tests | Yes |
| `sanitizer-asan` | AddressSanitizer | Yes |
| `sanitizer-ubsan` | UndefinedBehaviorSanitizer | Yes |
| `sanitizer-tsan` | ThreadSanitizer | Yes (concurrency components) |
| `header-check` | Verify headers compile standalone + include order | Yes |
| `strict-warnings` | Extended warning flags | Yes |
| `ci-success` | Gate job aggregating all results | Yes |

### Optional Jobs (Manual Trigger)

| Job | Purpose | Trigger |
|-----|---------|---------|
| `benchmarks-gcc` | GCC benchmark runs | `inputs.run_benchmarks` |
| `benchmarks-clang` | Clang benchmark runs | `inputs.run_benchmarks` |
| `benchmarks-msvc` | MSVC benchmark runs | `inputs.run_benchmarks` |
| `benchmark-summary` | Aggregate benchmark results | `inputs.run_benchmarks` |

---

## 6. Compiler Version Matrix

### 6.1 GCC Versions

| Version | C++ Standard | Runner | Role |
|---------|--------------|--------|------|
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

## 13. Benchmark Jobs

Benchmarks are triggered manually via `inputs.run_benchmarks`.

### 13.1 Benchmark Environment Variables

All benchmarks must support these canonical environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `FATP_BENCH_WARMUP_RUNS` | 3 | Warmup batches (not reported) |
| `FATP_BENCH_BATCHES` | 20 | Measured batches |
| `FATP_BENCH_NO_STABILIZE` | 1 | Disable CPU stabilization (CI) |
| `FATP_BENCH_OUTPUT_CSV` | (set) | CSV output path |

### 13.2 Benchmark Build Flags

```yaml
# Linux
g++-${{ matrix.version }} -std=c++${{ matrix.std }} \
  -O3 -DNDEBUG -mavx2 -mfma \
  -I./include/fat_p \
  ${{ env.BENCH_SRC }} -o bench_bin -pthread

# Windows
cl /nologo $stdFlag /W4 /wd4324 /O2 /DNDEBUG /arch:AVX2 /EHsc /permissive- \
  /I.\include\fat_p components\<Component>\benchmarks\benchmark_<Component>.cpp \
  /Fe:bench_bin.exe /link advapi32.lib
```

---

## 14. Output Format Requirements

### 14.1 ASCII Only

All CI output must be ASCII-only. No Unicode characters.

| Instead of | Use |
|------------|-----|
| ✓ | `[PASS]` or `[x]` |
| ✗ | `[FAIL]` or `[ ]` |
| ❌ | `[X]` or `[FAIL]` |
| ⚠ | `[WARNING]` or `[!]` |

### 14.2 Success Messages

Use simple text without emoji:
```yaml
echo "Header is self-contained"    # Good
echo "✓ Header is self-contained"  # Bad (Unicode)
```

---

## 15. Complete Workflow Template

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
# =============================================================================

name: <ComponentName> CI

on:
  workflow_dispatch:
    inputs:
      run_benchmarks:
        description: 'Run benchmarks'
        required: false
        default: 'false'
        type: boolean

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

Before committing a new workflow:

- [ ] File named `.github/workflows/<component-name>.yml`
- [ ] Header block with directory structure documented
- [ ] All paths use `include/fat_p/` and `components/<Component>/...`
- [ ] `env:` block defines HEADER, TEST_SRC, BENCH_SRC
- [ ] `linux-gcc` job with GCC 13 (C++20) and GCC 14 (C++23)
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
- [ ] Benchmark jobs use `inputs.run_benchmarks` condition
- [ ] ASCII-only output (no Unicode symbols)
- [ ] Tested locally or validated manually

---

## 17. Common Mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Wrong paths | `file not found` | Use `include/fat_p/` and `components/.../` |
| Missing `advapi32.lib` | `LNK2019: __imp_RegOpenKeyExA` | Add `/link advapi32.lib` |
| Missing `/wd4324` | `C4324: structure was padded` | Add `/wd4324` |
| Forward slashes on Windows | `file not found` | Use backslashes: `.\include\fat_p` |
| Unicode in output | Display issues in logs | Use ASCII: `[PASS]` not `✓` |
| Missing TSan in ci-success | Gate passes despite TSan failure | Add `sanitizer-tsan` to `needs` |
| Old `FAT_P/FAT_P/` paths | `file not found` | Update to new structure |
| C++17 testing | Wasted CI time | Remove, only test C++20/C++23 |

---

## 18. Changelog

### v3.0 (February 2026)
- **Breaking:** Updated directory structure from `FAT_P/FAT_P/...` to `include/fat_p/` and `components/.../`
- **Breaking:** Dropped C++17 support; now C++20/C++23 only
- Added Section 7: MSVC-specific requirements (`/wd4324`, `advapi32.lib`, backslash paths)
- Added Section 14: ASCII-only output requirements
- Added Section 17: Common mistakes reference
- Updated compiler matrix: GCC 13/14, Clang 16/17, MSVC C++20/C++23
- Removed all references to `ci_core17.yml` and C++17 gates

### v2.1 (January 2026)
- Updated layer integrity guidance for `FATP_META.layer`

### v2.0 (January 2026)
- Added project-wide verification workflows
- Added C++17 core compile gate (now removed in v3.0)

### v1.0 (December 2025)
- Initial CI Workflow Style Guide

---

*Fat-P CI Workflow Style Guide v3.0 -- February 2026*
