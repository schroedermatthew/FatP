# Fat-P CI Workflow Style Guide

**Status:** Active  
**Applies to:** All `.github/workflows/*.yml` files for Fat-P components  
**Authority:** Subordinate to the *Fat-P Library Development Guidelines*

---

## 1. Purpose

This guide standardizes GitHub Actions CI workflows for Fat-P library components. Consistent workflows ensure:

- Uniform quality gates across all components
- Predictable CI behavior for contributors
- Compliance with Fat-P Systemic Hygiene Policy (§6)

---

## 2. Directory Structure

All Fat-P components follow this layout:

```
FAT_P/FAT_P/fat_p/              # Headers (.h)
FAT_P/FAT_P/tests/              # Test files (test_*.cpp)
FAT_P/FAT_P/benchmarks/         # Benchmark files (benchmark_*.cpp)
.github/workflows/              # CI workflow files (*.yml)
```

**Critical:** Always use these paths in workflows. Never use flat paths.

---

## 3. Workflow File Template

### 3.1 Naming Convention

```
.github/workflows/<component-name>.yml
```

Examples:
- `aligned_vector.yml`
- `policy-iterator.yml`
- `small-vector.yml`

Use kebab-case. Match the component's conceptual name, not necessarily the header filename.

### 3.2 Required Header Block

```yaml
# =============================================================================
# .github/workflows/<component-name>.yml
# =============================================================================
# CI workflow for <ComponentName> component
#
# Directory structure:
#   Headers:    FAT_P/FAT_P/fat_p/<Header>.h
#   Tests:      FAT_P/FAT_P/tests/test_<Component>.cpp
#   Benchmarks: FAT_P/FAT_P/benchmarks/benchmark_<Component>.cpp
#
# Fat-P Guidelines compliance:
#   - Section 6.1: Include-all compile test
#   - Section 6.2: Header self-contained tests
#   - Section 6.4: Warning cleanliness (-Wall -Wextra -Wpedantic -Werror)
# =============================================================================
```

### 3.3 Required Environment Variables

```yaml
env:
  INCLUDE_DIR: FAT_P/FAT_P/fat_p
  TEST_SRC: FAT_P/FAT_P/tests/test_<Component>.cpp
  BENCH_SRC: FAT_P/FAT_P/benchmarks/benchmark_<Component>.cpp
```

---

## 4. Trigger Configuration

### 4.1 Standard Triggers

```yaml
on:
  push:
    branches: [main, master]
    paths:
      - 'FAT_P/FAT_P/fat_p/<Header>.h'
      - 'FAT_P/FAT_P/tests/test_<Component>.cpp'
      - 'FAT_P/FAT_P/benchmarks/benchmark_<Component>.cpp'
      - '.github/workflows/<component-name>.yml'
  pull_request:
    branches: [main, master]
    paths:
      # Same paths as push
  schedule:
    - cron: '0 2 * * 0'  # Weekly benchmarks at 2am Sunday UTC
  workflow_dispatch:
    inputs:
      run_benchmarks:
        description: 'Run benchmarks'
        required: false
        default: 'false'
        type: boolean
```

---

## 5. Required Jobs

Every workflow MUST include these jobs:

| Job | Purpose | Fat-P Compliance |
|-----|---------|------------------|
| `linux-gcc` | GCC 11/12/13 × C++17/20 matrix | §6.4 |
| `linux-clang` | Clang 14/15/16 × C++17/20 matrix | §6.4 |
| `windows-msvc` | MSVC C++17/20 matrix | §6.4 |
| `sanitizer-asan` | AddressSanitizer | Best practice |
| `sanitizer-ubsan` | UndefinedBehaviorSanitizer | Best practice |
| `sanitizer-tsan` | ThreadSanitizer | Best practice |
| `header-check` | Verify headers compile standalone | §6.2 |
| `strict-warnings` | Extended warning flags | §6.4 |
| `ci-success` | Gate job aggregating all results | Best practice |

### 5.1 Linux GCC Builds (Required)

```yaml
  linux-gcc:
    name: Linux GCC-${{ matrix.version }} C++${{ matrix.std }}
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        include:
          - version: 11
            std: 17
          - version: 12
            std: 17
          - version: 12
            std: 20
          - version: 13
            std: 17
          - version: 13
            std: 20

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
            -I./FAT_P/FAT_P/fat_p \
            FAT_P/FAT_P/tests/test_<Component>.cpp -o test_bin

      - name: Run tests
        run: ./test_bin
```

### 5.2 Linux Clang Builds (Required)

```yaml
  linux-clang:
    name: Linux Clang-${{ matrix.version }} C++${{ matrix.std }}
    runs-on: ubuntu-22.04
    strategy:
      fail-fast: false
      matrix:
        include:
          - version: 14
            std: 17
          - version: 15
            std: 17
          - version: 15
            std: 20
          - version: 16
            std: 17
          - version: 16
            std: 20

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
            -O2 -DNDEBUG \
            -DENABLE_TEST_APPLICATION \
            -I./FAT_P/FAT_P/fat_p \
            FAT_P/FAT_P/tests/test_<Component>.cpp -o test_bin

      - name: Run tests
        run: ./test_bin
```

**Note:** Add `-Wno-gnu-zero-variadic-macro-arguments` if `enforce.h` triggers GNU extension warnings.

### 5.3 Windows MSVC Builds (Required)

```yaml
  windows-msvc:
    name: Windows MSVC C++${{ matrix.std }}
    runs-on: windows-latest
    strategy:
      fail-fast: false
      matrix:
        std: [17, 20]

    steps:
      - uses: actions/checkout@v4

      - name: Setup MSVC
        uses: ilammy/msvc-dev-cmd@v1

      - name: Build tests
        run: |
          cl /std:c++${{ matrix.std }} /W4 /WX /EHsc /permissive- /O2 /DNDEBUG /DENABLE_TEST_APPLICATION /I.\FAT_P\FAT_P\fat_p FAT_P\FAT_P\tests\test_<Component>.cpp /Fe:test_bin.exe

      - name: Run tests
        run: .\test_bin.exe
```

### 5.4 Sanitizer Jobs (Required)

```yaml
  sanitizer-asan:
    name: AddressSanitizer
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Build with ASan
        run: |
          g++ -std=c++17 -Wall -Wextra -g -O1 \
            -fsanitize=address -fno-omit-frame-pointer \
            -DENABLE_TEST_APPLICATION \
            -I./FAT_P/FAT_P/fat_p \
            FAT_P/FAT_P/tests/test_<Component>.cpp -o test_bin

      - name: Run with ASan
        env:
          ASAN_OPTIONS: detect_leaks=1:abort_on_error=1
        run: ./test_bin

  sanitizer-ubsan:
    name: UndefinedBehaviorSanitizer
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Build with UBSan
        run: |
          g++ -std=c++17 -Wall -Wextra -g -O1 \
            -fsanitize=undefined -fno-omit-frame-pointer \
            -DENABLE_TEST_APPLICATION \
            -I./FAT_P/FAT_P/fat_p \
            FAT_P/FAT_P/tests/test_<Component>.cpp -o test_bin

      - name: Run with UBSan
        env:
          UBSAN_OPTIONS: print_stacktrace=1:halt_on_error=1
        run: ./test_bin

  sanitizer-tsan:
    name: ThreadSanitizer
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Build with TSan
        run: |
          g++ -std=c++17 -Wall -Wextra -g -O1 \
            -fsanitize=thread -fno-omit-frame-pointer \
            -DENABLE_TEST_APPLICATION \
            -I./FAT_P/FAT_P/fat_p \
            FAT_P/FAT_P/tests/test_<Component>.cpp -o test_bin

      - name: Run with TSan
        env:
          TSAN_OPTIONS: halt_on_error=1
        run: ./test_bin
```

### 5.5 Header Self-Containment Job (Required)

```yaml
  header-check:
    name: Header Self-Containment
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Test <Header>.h compiles standalone
        run: |
          echo '#include "<Header>.h"' > test_include.cpp
          echo 'int main() { return 0; }' >> test_include.cpp
          
          g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror \
            -I./FAT_P/FAT_P/fat_p \
            -c test_include.cpp -o /dev/null
          
          echo "✓ Header is self-contained"

      - name: Test include order independence
        run: |
          # <Header> first
          cat > test1.cpp << 'EOF'
          #include "<Header>.h"
          #include <vector>
          #include <algorithm>
          int main() { /* minimal usage */ return 0; }
          EOF
          
          # <Header> last
          cat > test2.cpp << 'EOF'
          #include <algorithm>
          #include <vector>
          #include "<Header>.h"
          int main() { /* minimal usage */ return 0; }
          EOF
          
          g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I./FAT_P/FAT_P/fat_p -c test1.cpp -o /dev/null
          g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I./FAT_P/FAT_P/fat_p -c test2.cpp -o /dev/null
          
          echo "✓ Include order independent"
```

### 5.6 Strict Warnings Job (Required)

```yaml
  strict-warnings:
    name: Strict Warnings
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Compile with strict warnings
        run: |
          g++ -std=c++17 \
              -Wall -Wextra -Wpedantic \
              -Wconversion -Wsign-conversion \
              -Wshadow -Wformat=2 \
              -Werror \
              -DENABLE_TEST_APPLICATION -I./FAT_P/FAT_P/fat_p \
              -o test_strict FAT_P/FAT_P/tests/test_<Component>.cpp
          echo "✓ No warnings"
```

### 5.7 CI Gate Job (Required)

```yaml
  ci-success:
    name: CI Success
    needs: [linux-gcc, linux-clang, windows-msvc, sanitizer-asan, sanitizer-ubsan, header-check, strict-warnings]
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
          if [[ "${{ needs.header-check.result }}" != "success" ]]; then exit 1; fi
          if [[ "${{ needs.strict-warnings.result }}" != "success" ]]; then exit 1; fi
          echo "✓ All checks passed"
```

**Note:** TSan is not included in the gate by default as it may have false positives on some components. Include it when the component has explicit threading code.

---

## 6. Benchmarks (Weekly/Manual)

```yaml
  benchmarks:
    name: Benchmarks
    runs-on: ubuntu-latest
    if: ${{ github.event_name == 'schedule' || github.event.inputs.run_benchmarks == 'true' }}
    steps:
      - uses: actions/checkout@v4

      - name: Build benchmark
        run: |
          g++ -std=c++17 -O3 -DNDEBUG -march=native \
            -I./FAT_P/FAT_P/fat_p \
            FAT_P/FAT_P/benchmarks/benchmark_<Component>.cpp -o bench_bin

      - name: Run benchmarks
        env:
          FATP_BENCH_WARMUP_RUNS: 3
          FATP_BENCH_BATCHES: 20
          FATP_BENCH_NO_STABILIZE: 1
          FATP_BENCH_OUTPUT_CSV: results.csv
        run: ./bench_bin 2>&1 | tee benchmark.log

      - name: Upload results
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-results
          path: |
            results.csv
            benchmark.log
```

---

## 7. Compiler Flags Reference

### 7.1 Required Warning Flags

| Compiler | Minimum Flags |
|----------|---------------|
| GCC/Clang | `-Wall -Wextra -Wpedantic -Werror` |
| MSVC | `/W4 /WX /permissive-` |

### 7.2 Extended Warning Flags (strict-warnings job)

```
-Wconversion -Wsign-conversion -Wshadow -Wformat=2
```

### 7.3 Clang-Specific Suppressions

If `enforce.h` triggers GNU extension warnings:

```yaml
-Wno-gnu-zero-variadic-macro-arguments
```

### 7.4 Build Configurations

| Configuration | Flags |
|---------------|-------|
| Release | `-O2 -DNDEBUG -DENABLE_TEST_APPLICATION` |
| Sanitizer | `-g -O1 -fsanitize=<type> -fno-omit-frame-pointer` |
| Benchmark | `-O3 -DNDEBUG -march=native` |

---

## 8. Compiler Version Matrix

### 8.1 Required GCC Versions

| Version | C++ Standards |
|---------|---------------|
| GCC 11 | C++17 |
| GCC 12 | C++17, C++20 |
| GCC 13 | C++17, C++20 |

### 8.2 Required Clang Versions

| Version | C++ Standards |
|---------|---------------|
| Clang 14 | C++17 |
| Clang 15 | C++17, C++20 |
| Clang 16 | C++17, C++20 |

### 8.3 Required MSVC Standards

| Standard |
|----------|
| C++17 |
| C++20 |

---

## 9. Checklist for New Workflows

Before committing a new workflow:

- [ ] File named `.github/workflows/<component-name>.yml`
- [ ] Header block with directory structure documented
- [ ] All paths use `FAT_P/FAT_P/fat_p/` prefix
- [ ] `env:` block defines INCLUDE_DIR, TEST_SRC, BENCH_SRC
- [ ] `linux-gcc` job with GCC 11/12/13 × C++17/20 matrix
- [ ] `linux-clang` job with Clang 14/15/16 × C++17/20 matrix
- [ ] `windows-msvc` job with C++17/20 matrix
- [ ] `sanitizer-asan` job
- [ ] `sanitizer-ubsan` job
- [ ] `sanitizer-tsan` job
- [ ] `header-check` job
- [ ] `strict-warnings` job
- [ ] `benchmarks` job (weekly + manual trigger)
- [ ] `ci-success` gate job
- [ ] Path triggers include the workflow file itself
- [ ] `schedule` trigger for weekly benchmarks
- [ ] Tested locally with `act` or manual validation

---

## 10. Common Mistakes

| Mistake | Correct Pattern |
|---------|-----------------|
| `test_Foo.cpp` (flat path) | `FAT_P/FAT_P/tests/test_Foo.cpp` |
| Missing `-I` flag | `-I./FAT_P/FAT_P/fat_p` |
| MSVC forward slashes | Use backslashes: `FAT_P\FAT_P\fat_p` |
| Forgetting `workflow_dispatch` | Always include for manual runs |
| Single compiler version | Use full GCC/Clang/MSVC matrix |
| Combined sanitizers | Use separate jobs for ASan/UBSan/TSan |
| Missing `fail-fast: false` | Add to strategy to run all matrix entries |
| No CI gate job | Always add `ci-success` job |

---

## 11. Reference Implementations

See these workflows for complete examples:

- `aligned_vector.yml` — Full reference implementation
- `policy-iterator.yml` — Multi-header component example
