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
  workflow_dispatch:
    inputs:
      run_benchmarks:
        description: 'Run benchmarks'
        required: false
        default: 'false'
        type: boolean
```

### 4.2 Optional: Scheduled Benchmarks

```yaml
  schedule:
    - cron: '0 2 * * 0'  # Weekly at 2am Sunday UTC
```

---

## 5. Required Jobs

Every workflow MUST include these jobs:

| Job | Purpose | Fat-P Compliance |
|-----|---------|------------------|
| `header-check` | Verify headers compile standalone | §6.2 |
| `test-gcc` | GCC debug + release tests | §6.4 |
| `test-clang` | Clang debug + release tests | §6.4 |
| `sanitizers` | ASan + UBSan validation | Best practice |
| `strict-warnings` | Extended warning flags | §6.4 |

### 5.1 Header Self-Containment Job

```yaml
  header-check:
    name: Header Self-Containment
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Check <Header>.h
        run: |
          echo '#include "<Header>.h"' | \
            g++ -std=c++17 -fsyntax-only -I./FAT_P/FAT_P/fat_p -x c++ -
          echo "✓ <Header>.h compiles standalone"
      
      - name: Include-order stress test
        run: |
          cat > stress_test.cpp << 'EOF'
          #include <vector>
          #include <algorithm>
          #include "<Header>.h"
          #include <map>
          int main() { /* minimal usage test */ return 0; }
          EOF
          g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror \
            -I./FAT_P/FAT_P/fat_p -o stress_test stress_test.cpp
          ./stress_test
```

### 5.2 Compiler Test Jobs

```yaml
  test-gcc:
    name: GCC ${{ matrix.build_type }}
    runs-on: ubuntu-latest
    strategy:
      matrix:
        build_type: [Debug, Release]
        include:
          - build_type: Debug
            flags: "-g -DENABLE_TEST_APPLICATION"
          - build_type: Release
            flags: "-O3 -DNDEBUG -DENABLE_TEST_APPLICATION"
    steps:
      - uses: actions/checkout@v4
      
      - name: Build
        run: |
          g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror \
              ${{ matrix.flags }} -I./FAT_P/FAT_P/fat_p \
              -o test_runner FAT_P/FAT_P/tests/test_<Component>.cpp
      
      - name: Run tests
        run: ./test_runner
```

### 5.3 Sanitizer Job

```yaml
  sanitizers:
    name: Sanitizers (ASan + UBSan)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Build with sanitizers
        run: |
          g++ -std=c++17 -g \
              -fsanitize=address,undefined \
              -fno-omit-frame-pointer \
              -DENABLE_TEST_APPLICATION -I./FAT_P/FAT_P/fat_p \
              -o test_san FAT_P/FAT_P/tests/test_<Component>.cpp
      
      - name: Run sanitized tests
        run: ./test_san
```

### 5.4 Strict Warnings Job

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
```

---

## 6. Optional Jobs

### 6.1 Multi-Version Compiler Matrix

For thorough compatibility testing:

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
```

### 6.2 Windows MSVC

```yaml
  windows-msvc:
    name: Windows MSVC C++${{ matrix.std }}
    runs-on: windows-latest
    strategy:
      matrix:
        std: [17, 20]
    steps:
      - uses: actions/checkout@v4
      - name: Setup MSVC
        uses: ilammy/msvc-dev-cmd@v1
      - name: Build tests
        run: |
          cl /std:c++${{ matrix.std }} /W4 /WX /EHsc /permissive- /O2 /DNDEBUG ^
            /I.\FAT_P\FAT_P\fat_p ^
            FAT_P\FAT_P\tests\test_<Component>.cpp /Fe:test.exe
      - name: Run tests
        run: .\test.exe
```

### 6.3 Benchmarks (Gated)

```yaml
  benchmarks:
    name: Benchmarks
    runs-on: ubuntu-latest
    if: github.event_name == 'workflow_dispatch' && github.event.inputs.run_benchmarks == 'true'
    steps:
      - uses: actions/checkout@v4
      
      - name: Build benchmark
        run: |
          g++ -std=c++17 -O3 -DNDEBUG -march=native \
            -I./FAT_P/FAT_P/fat_p \
            FAT_P/FAT_P/benchmarks/benchmark_<Component>.cpp -o benchmark
      
      - name: Run benchmark
        run: ./benchmark
      
      - name: Upload results
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-results
          path: benchmark_results.csv
          if-no-files-found: ignore
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
| Debug | `-g -DENABLE_TEST_APPLICATION` |
| Release | `-O3 -DNDEBUG -DENABLE_TEST_APPLICATION` |
| Sanitizer | `-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer` |
| Benchmark | `-O3 -DNDEBUG -march=native` |

---

## 8. Checklist for New Workflows

Before committing a new workflow:

- [ ] File named `.github/workflows/<component-name>.yml`
- [ ] Header block with directory structure documented
- [ ] All paths use `FAT_P/FAT_P/fat_p/` prefix
- [ ] `env:` block defines INCLUDE_DIR, TEST_SRC, BENCH_SRC
- [ ] `header-check` job included
- [ ] `test-gcc` job with Debug/Release matrix
- [ ] `test-clang` job with Debug/Release matrix
- [ ] `sanitizers` job (ASan + UBSan)
- [ ] `strict-warnings` job
- [ ] Path triggers include the workflow file itself
- [ ] Tested locally with `act` or manual validation

---

## 9. Common Mistakes

| Mistake | Correct Pattern |
|---------|-----------------|
| `test_Foo.cpp` (flat path) | `FAT_P/FAT_P/tests/test_Foo.cpp` |
| Missing `-I` flag | `-I./FAT_P/FAT_P/fat_p` |
| MSVC forward slashes | Use backslashes: `FAT_P\FAT_P\fat_p` |
| Forgetting `workflow_dispatch` | Always include for manual runs |
| Hard-coding compiler version | Use matrix for flexibility |

---

## 10. Example: Minimal Complete Workflow

See `policy-iterator.yml` or `aligned_vector.yml` in the repository for complete reference implementations.
