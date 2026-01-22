# Fat-P CI Workflow Style Guide

**Status:** Active  
**Applies to:** All `.github/workflows/*.yml` files for Fat-P components  
**Authority:** Subordinate to the *Fat-P Library Development Guidelines*
**Version:** 2.1 (January 2026)

---

## 1. Purpose

This guide standardizes GitHub Actions CI workflows for Fat-P library components. Consistent workflows ensure:

- Uniform quality gates across all components
- Predictable CI behavior for contributors
- Compliance with Fat-P Systemic Hygiene Policy (§6)
- **Enforcement of C++20 default / C++17 core guarantee** (Guidelines §1.1)

---

## 2. Directory Structure

All Fat-P components follow this layout:

```
FAT_P/FAT_P/fat_p/              # Headers (.h)
FAT_P/FAT_P/tests/              # Test files (test_*.cpp)
FAT_P/FAT_P/benchmarks/         # Benchmark files (benchmark_*.cpp)
.github/workflows/              # CI workflow files (*.yml)
scripts/                        # Verification scripts
```

**Critical:** Always use these paths in workflows. Never use flat paths.

---

## 3. CI Goals

| Goal | Implementation |
|------|----------------|
| Default build/test | C++20 full matrix |
| Compile-only gate | C++17 for Foundation/Containers/Concurrency |
| Layer integrity | Verify `FATP_META.layer` and include direction |
| Dependency hygiene | Verify no forbidden third-party includes |

---

## 4. Required Jobs

Every component workflow MUST include these jobs:

| Job | Purpose | Required |
|-----|---------|----------|
| `linux-gcc` | GCC 13 (C++20) build + tests | ✅ |
| `linux-clang` | Clang 16 (C++20) build + tests | ✅ |
| `windows-msvc` | MSVC (C++20) build + tests | ✅ |
| `sanitizer-asan` | AddressSanitizer (C++20) | ✅ |
| `sanitizer-ubsan` | UndefinedBehaviorSanitizer (C++20) | ✅ |
| `sanitizer-tsan` | ThreadSanitizer (C++20) | ✅ (Concurrency-relevant components) |
| `header-check` | Verify public headers compile standalone (C++20) | ✅ |
| `strict-warnings` | Extended warning flags (C++20) | ✅ |
| `ci-success` | Gate job aggregating all results | ✅ |

Notes:
- C++17 compatibility is enforced **project-wide** by `ci_core17.yml` for the core layers (Foundation/Containers/Concurrency).
- Component workflows should not run a full C++17 matrix unless the component lives in a C++17-guaranteed layer and the additional coverage is worth the CI cost.

### New Required Jobs (v2.0)

| Job | Purpose | Required |
|-----|---------|----------|
| `cpp17-core-gate` | C++17 compile gate for core layers | ✅ (project-wide) |
| `layer-verify` | Verify `FATP_META.layer` matches includes | ✅ (project-wide) |
| `forbidden-deps` | Scan for Boost/Abseil/fmt/Eigen includes | ✅ (project-wide) |

---

## 5. Project-Wide Verification Workflows

These workflows run once per push/PR, not per-component.

### 5.1 C++17 Core Compile Gate (`ci_core17.yml`)

Verifies that Foundation, Containers, and Concurrency layers compile under C++17.

```yaml
# =============================================================================
# .github/workflows/ci_core17.yml
# =============================================================================
# C++17 compile gate for core layers (Foundation/Containers/Concurrency)
# Per Fat-P Guidelines §1.1.1: These layers must compile under C++17
# =============================================================================

name: C++17 Core Compile Gate

on:
  push:
    branches: [main, master]
  pull_request:
    branches: [main, master]

jobs:
  # ===========================================================================
  # GCC C++17 Core Compile
  # ===========================================================================
  compile-core-gcc:
    name: C++17 Core (GCC-${{ matrix.version }})
    runs-on: ${{ matrix.runner }}
    strategy:
      fail-fast: false
      matrix:
        include:
          - version: 11
            runner: ubuntu-22.04
          - version: 12
            runner: ubuntu-22.04
          - version: 13
            runner: ubuntu-24.04

    steps:
      - uses: actions/checkout@v4

      - name: Install GCC
        run: sudo apt-get update && sudo apt-get install -y g++-${{ matrix.version }}

      - name: Compile Foundation/Containers/Concurrency headers
        run: |
          echo "Compiling core layer headers under C++17..."
          
          HEADER_DIR="FAT_P/FAT_P/fat_p"
          if [ ! -d "$HEADER_DIR" ]; then
            echo "❌ Missing header directory: $HEADER_DIR"
            exit 1
          fi
          shopt -s nullglob
          all_headers=("$HEADER_DIR"/*.h)
          if [ ${#all_headers[@]} -eq 0 ]; then
            echo "❌ No headers found in $HEADER_DIR"
            exit 1
          fi

          # List of core layer headers
          # New layer names: Foundation, Containers, Concurrency
          # Legacy names: Infrastructure, CoreUtility, Enforcement
          CORE_HEADERS=$(grep -l -E '^[[:space:]]*layer:[[:space:]]*(Foundation|Containers|Concurrency|Infrastructure|CoreUtility|Enforcement)[[:space:]]*$' "${all_headers[@]}" 2>/dev/null || true)
          
          if [ -z "$CORE_HEADERS" ]; then
            echo "⚠ No core layer headers found with FATP_META.layer"
            echo "Checking all headers as fallback..."
            CORE_HEADERS="${all_headers[*]}"
          fi
          
          success=0
          failed=0
          
          for header in $CORE_HEADERS; do
            basename=$(basename "$header")
            cat > test_compile.cpp << EOF
          #include "$basename"
          int main() { return 0; }
          EOF
            if g++-${{ matrix.version }} -std=c++17 -Wall -Wextra -Wpedantic -Werror \
              -I./FAT_P/FAT_P/fat_p -I./FAT_P/FAT_P -c test_compile.cpp -o /dev/null 2>/dev/null; then
              echo "✓ $basename"
              success=$((success + 1))
            else
              echo "✗ $basename (C++17 compile failed)"
              failed=$((failed + 1))
            fi
          done
          
          echo ""
          echo "Results: $success passed, $failed failed"
          
          if [ $failed -gt 0 ]; then
            echo "❌ Some core layer headers failed C++17 compilation"
            exit 1
          fi
          
          echo "✓ All core layer headers compile under C++17"

  # ===========================================================================
  # Clang C++17 Core Compile
  # ===========================================================================
  compile-core-clang:
    name: C++17 Core (Clang-${{ matrix.version }})
    runs-on: ubuntu-22.04
    strategy:
      fail-fast: false
      matrix:
        version: [14, 15, 16]

    steps:
      - uses: actions/checkout@v4

      - name: Install Clang
        run: |
          wget https://apt.llvm.org/llvm.sh
          chmod +x llvm.sh
          sudo ./llvm.sh ${{ matrix.version }}

      - name: Compile Foundation/Containers/Concurrency headers
        run: |
          echo "Compiling core layer headers under C++17..."
          
          HEADER_DIR="FAT_P/FAT_P/fat_p"
          if [ ! -d "$HEADER_DIR" ]; then
            echo "❌ Missing header directory: $HEADER_DIR"
            exit 1
          fi
          shopt -s nullglob
          all_headers=("$HEADER_DIR"/*.h)
          if [ ${#all_headers[@]} -eq 0 ]; then
            echo "❌ No headers found in $HEADER_DIR"
            exit 1
          fi

          CORE_HEADERS=$(grep -l -E '^[[:space:]]*layer:[[:space:]]*(Foundation|Containers|Concurrency|Infrastructure|CoreUtility|Enforcement)[[:space:]]*$' "${all_headers[@]}" 2>/dev/null || true)
          
          if [ -z "$CORE_HEADERS" ]; then
            echo "⚠ No core layer headers found with FATP_META.layer"
            CORE_HEADERS="${all_headers[*]}"
          fi
          
          success=0
          failed=0
          
          for header in $CORE_HEADERS; do
            basename=$(basename "$header")
            cat > test_compile.cpp << EOF
          #include "$basename"
          int main() { return 0; }
          EOF
            if clang++-${{ matrix.version }} -std=c++17 -Wall -Wextra -Wpedantic -Werror \
              -Wno-gnu-zero-variadic-macro-arguments \
              -I./FAT_P/FAT_P/fat_p -I./FAT_P/FAT_P -c test_compile.cpp -o /dev/null 2>/dev/null; then
              echo "✓ $basename"
              success=$((success + 1))
            else
              echo "✗ $basename (C++17 compile failed)"
              failed=$((failed + 1))
            fi
          done
          
          echo ""
          echo "Results: $success passed, $failed failed"
          
          if [ $failed -gt 0 ]; then
            echo "❌ Some core layer headers failed C++17 compilation"
            exit 1
          fi
          
          echo "✓ All core layer headers compile under C++17"

  # ===========================================================================
  # Gate Job
  # ===========================================================================
  core17-success:
    name: C++17 Core Gate Success
    needs: [compile-core-gcc, compile-core-clang]
    runs-on: ubuntu-latest
    if: always()
    steps:
      - name: Check results
        run: |
          if [[ "${{ needs.compile-core-gcc.result }}" != "success" ]]; then
            echo "❌ GCC C++17 core compile failed"
            exit 1
          fi
          if [[ "${{ needs.compile-core-clang.result }}" != "success" ]]; then
            echo "❌ Clang C++17 core compile failed"
            exit 1
          fi
          echo "✓ C++17 core compile gate passed"

```

### 5.2 Layer Verification (`ci_verify.yml`)

Verifies that `FATP_META.layer` matches actual include dependencies.

```yaml
# =============================================================================
# .github/workflows/ci_verify.yml
# =============================================================================
# Verifies FATP_META.layer matches actual #include dependencies and no forbidden deps
# Per Fat-P Guidelines §2.2: Layer mismatches are Critical violations
# Per Fat-P Guidelines §1.6: No third-party libraries
# =============================================================================

name: Layer & Dependency Verification

on:
  push:
    branches: [main, master]
  pull_request:
    branches: [main, master]

jobs:
  # ===========================================================================
  # Layer Tag Verification
  # ===========================================================================
  verify-layers:
    name: Verify Layer Dependencies
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Check FATP_META.layer present
        run: |
          echo "Checking all headers have FATP_META.layer..."
          
          found=0
          missing=0
          for header in FAT_P/FAT_P/fat_p/*.h; do
            if [ ! -f "$header" ]; then
              continue
            fi
            found=$((found + 1))
            if ! grep -q 'FATP_META:' "$header"; then
              echo "❌ Missing FATP_META block: $header"
              missing=$((missing + 1))
              continue
            fi
            if ! grep -q -E '^[[:space:]]*layer:[[:space:]]*' "$header"; then
              echo "❌ Missing FATP_META.layer: $header"
              missing=$((missing + 1))
            fi
          done

          if [ $found -eq 0 ]; then
            echo "❌ No headers found under FAT_P/FAT_P/fat_p/*.h"
            exit 1
          fi
          
          if [ $missing -gt 0 ]; then
            echo ""
            echo "❌ $missing headers missing FATP_META.layer"
            echo "Add layer to FATP_META per Guidelines §2.2"
            exit 1
          fi
          
          echo "✓ All headers have FATP_META.layer"

      - name: Verify layer dependencies
        run: |
          echo "Verifying layer dependency order..."
          
          # Layer order (lower = can be included by higher)
          # Foundation(0) < Containers(1) < Concurrency(2) < Domain(3) < Integration(4) < Testing(5)
          
          get_layer_level() {
            case "$1" in
              Foundation|CoreUtility|Enforcement) echo 0 ;;
              Containers|Infrastructure) echo 1 ;;
              Concurrency) echo 2 ;;
              Domain|Policy|Application|Serialization) echo 3 ;;
              Integration) echo 4 ;;
              Testing) echo 5 ;;
              *) echo 99 ;;
            esac
          }
          
          violations=0
          found=0
          
          for header in FAT_P/FAT_P/fat_p/*.h; do
            if [ ! -f "$header" ]; then
              continue
            fi
            found=$((found + 1))
            
            basename=$(basename "$header")
            
            # Extract FATP_META.layer
            layer=$(grep -oP '^[[:space:]]*layer:\s*\K\w+' "$header" 2>/dev/null | head -1 || echo "MISSING")
            
            if [ "$layer" = "MISSING" ]; then
              continue  # Already reported above
            fi
            
            layer_level=$(get_layer_level "$layer")
            
            if [ "$layer_level" = "99" ]; then
              echo "❌ $basename: Unknown layer '$layer'"
              violations=$((violations + 1))
              continue
            fi
            
            # Check internal includes
            while IFS= read -r inc; do
              inc_file=$(echo "$inc" | grep -oP '"[^"]+"' | tr -d '"')
              # Normalize includes like "fat_p/Foo.h" to "Foo.h"
              inc_file=$(basename "$inc_file")
              inc_path="FAT_P/FAT_P/fat_p/$inc_file"
              
              if [ ! -f "$inc_path" ]; then
                continue  # External include
              fi
              
              inc_layer=$(grep -oP '^[[:space:]]*layer:\s*\K\w+' "$inc_path" 2>/dev/null | head -1 || echo "MISSING")
              inc_level=$(get_layer_level "$inc_layer")
              
              if [ "$inc_level" -gt "$layer_level" ]; then
                echo "❌ LAYER VIOLATION: $basename (layer $layer, level $layer_level)"
                echo "   includes $inc_file (layer $inc_layer, level $inc_level)"
                violations=$((violations + 1))
              fi
            done < <(grep -E '^[[:space:]]*#include[[:space:]]*"' "$header" || true)
          done

          if [ $found -eq 0 ]; then
            echo "❌ No headers found under FAT_P/FAT_P/fat_p/*.h"
            exit 1
          fi
          
          if [ $violations -gt 0 ]; then
            echo ""
            echo "❌ $violations layer violations found"
            echo "Components may only include headers from lower or equal layers"
            exit 1
          fi
          
          echo "✓ All layer dependencies valid"

  # ===========================================================================
  # Forbidden Dependency Scan
  # ===========================================================================
  scan-forbidden-deps:
    name: Scan Forbidden Dependencies
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Check for forbidden includes
        run: |
          echo "Scanning for forbidden third-party includes..."
          echo "Per Guidelines §1.6: No third-party libraries"
          echo ""
          
          violations=0
          
          # Check for Boost
          matches=$(grep -rn '#include.*[<"]boost/' FAT_P/FAT_P/fat_p/ 2>/dev/null || true)
          if [ -n "$matches" ]; then
            echo "❌ Forbidden: Boost"
            echo "$matches"
            violations=$((violations + 1))
          fi
          
          # Check for Abseil
          matches=$(grep -rn '#include.*[<"]absl/' FAT_P/FAT_P/fat_p/ 2>/dev/null || true)
          if [ -n "$matches" ]; then
            echo "❌ Forbidden: Abseil"
            echo "$matches"
            violations=$((violations + 1))
          fi
          
          # Check for fmt
          matches=$(grep -rn '#include.*[<"]fmt/' FAT_P/FAT_P/fat_p/ 2>/dev/null || true)
          if [ -n "$matches" ]; then
            echo "❌ Forbidden: fmt"
            echo "$matches"
            violations=$((violations + 1))
          fi
          
          # Check for Eigen
          matches=$(grep -rn '#include.*[<"]Eigen/' FAT_P/FAT_P/fat_p/ 2>/dev/null || true)
          if [ -n "$matches" ]; then
            echo "❌ Forbidden: Eigen"
            echo "$matches"
            violations=$((violations + 1))
          fi
          
          # Check for nlohmann/json
          matches=$(grep -rn '#include.*[<"]nlohmann/' FAT_P/FAT_P/fat_p/ 2>/dev/null || true)
          if [ -n "$matches" ]; then
            echo "❌ Forbidden: nlohmann/json"
            echo "$matches"
            violations=$((violations + 1))
          fi
          
          # Check for spdlog
          matches=$(grep -rn '#include.*[<"]spdlog/' FAT_P/FAT_P/fat_p/ 2>/dev/null || true)
          if [ -n "$matches" ]; then
            echo "❌ Forbidden: spdlog"
            echo "$matches"
            violations=$((violations + 1))
          fi
          
          # Check for catch2/gtest/doctest (in main headers, not tests)
          matches=$(grep -rn '#include.*[<"]catch2/\|#include.*[<"]gtest/\|#include.*[<"]doctest/' FAT_P/FAT_P/fat_p/ 2>/dev/null || true)
          if [ -n "$matches" ]; then
            echo "❌ Forbidden: Test frameworks in library headers"
            echo "$matches"
            violations=$((violations + 1))
          fi
          
          if [ $violations -gt 0 ]; then
            echo ""
            echo "❌ $violations forbidden dependency violations found"
            echo "Fat-P must not depend on third-party libraries"
            exit 1
          fi
          
          echo "✓ No forbidden dependencies found"

  # ===========================================================================
  # Gate Job
  # ===========================================================================
  verify-success:
    name: Verification Success
    needs: [verify-layers, scan-forbidden-deps]
    runs-on: ubuntu-latest
    if: always()
    steps:
      - name: Check results
        run: |
          if [[ "${{ needs.verify-layers.result }}" != "success" ]]; then
            echo "❌ Layer verification failed"
            exit 1
          fi
          if [[ "${{ needs.scan-forbidden-deps.result }}" != "success" ]]; then
            echo "❌ Forbidden dependency scan failed"
            exit 1
          fi
          echo "✓ All verification checks passed"

```

### 5.3 Layer Verification Script (`scripts/verify_layers.sh`)

```bash
#!/bin/bash
# =============================================================================
# scripts/verify_layers.sh
# =============================================================================
# Verifies FATP_META.layer matches actual #include dependencies
# =============================================================================

set -e

# Layer order (lower index = lower layer)
declare -A LAYER_ORDER=(
    ["Foundation"]=0
    ["Containers"]=1
    ["Concurrency"]=2
    ["Domain"]=3
    ["Integration"]=4
    ["Testing"]=5
    # Legacy mappings
    ["Infrastructure"]=1
    ["CoreUtility"]=0
    ["Enforcement"]=0
    ["Policy"]=3
    ["Application"]=3
    ["Serialization"]=3
)

HEADER_DIR="FAT_P/FAT_P/fat_p"

if [ ! -d "$HEADER_DIR" ]; then
    echo "❌ Missing header directory: $HEADER_DIR"
    exit 1
fi

shopt -s nullglob
headers=("$HEADER_DIR"/*.h)
if [ ${#headers[@]} -eq 0 ]; then
    echo "❌ No headers found in $HEADER_DIR"
    exit 1
fi

violations=0

for header in "${headers[@]}"; do
    basename=$(basename "$header")

    # Extract FATP_META.layer
    layer=$(grep -oP '^[[:space:]]*layer:\s*\K\w+' "$header" 2>/dev/null | head -1 || true)

    if [ -z "$layer" ]; then
        echo "❌ $basename: Missing FATP_META.layer"
        violations=$((violations + 1))
        continue
    fi

    layer_level=${LAYER_ORDER[$layer]:-99}
    if [ "$layer_level" = "99" ]; then
        echo "❌ $basename: Unknown layer '$layer'"
        violations=$((violations + 1))
        continue
    fi

    # Extract internal #includes (Fat-P headers only)
    includes=$(grep -oP '#include\s+"([^"]+)"' "$header" | grep -oP '"[^"]+"' | tr -d '"' || true)

    for inc in $includes; do
        inc_basename=$(basename "$inc")
        inc_path="$HEADER_DIR/$inc_basename"
        if [ ! -f "$inc_path" ]; then
            continue  # External include, skip
        fi

        inc_layer=$(grep -oP '^[[:space:]]*layer:\s*\K\w+' "$inc_path" 2>/dev/null | head -1 || true)
        if [ -z "$inc_layer" ]; then
            echo "❌ $basename includes $inc_basename which is missing FATP_META.layer"
            violations=$((violations + 1))
            continue
        fi

        inc_level=${LAYER_ORDER[$inc_layer]:-99}

        if [ "$inc_level" -gt "$layer_level" ]; then
            echo "❌ $basename (layer $layer) includes $inc_basename (layer $inc_layer) - LAYER VIOLATION"
            violations=$((violations + 1))
        fi
    done
done

if [ $violations -gt 0 ]; then
    echo ""
    echo "❌ $violations layer violations found"
    exit 1
fi

echo "✓ All layer dependencies valid"
exit 0

```

---

## 6. Component Workflow Template

### 6.1 Header Block (Updated)

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
#   - Section 1.1: C++20 default, C++17 for core layers
#   - Section 2.2: Layer verification via FATP_META.layer
#   - Section 6.1: Include-all compile test
#   - Section 6.2: Header self-contained tests
#   - Section 6.4: Warning cleanliness (-Wall -Wextra -Wpedantic -Werror)
# =============================================================================
```

### 6.2 Build Matrix (Updated)

**C++20 is the standard for component workflows.**

C++17 compatibility is enforced **project-wide** via `ci_core17.yml` (bounded to Foundation/Containers/Concurrency).
Component workflows should not add C++17 jobs unless the component lives in a C++17-guaranteed layer and the extra coverage is worth the CI cost.

```yaml
    strategy:
      fail-fast: false
      matrix:
        include:
          # C++20 (primary)
          - version: 13
            std: 20
          - version: 12
            std: 20
```

---

## 7. Compiler Version Matrix

### 7.1 Required GCC Versions

Fat-P uses two compiler matrices:
- **Component workflows (C++20 default):** validate primary C++20 builds.
- **Project-wide core C++17 gate:** validates bounded C++17 compatibility for Foundation/Containers/Concurrency.

Note: GCC 11/12 are typically run on Ubuntu 22.04; GCC 13 is typically run on Ubuntu 24.04, to match package availability on GitHub runners.

| Version | C++ Standards | Notes |
|---------|---------------|-------|
| GCC 11 | C++17 | Minimum supported |
| GCC 12 | C++17, C++20 | |
| GCC 13 | C++17, C++20 | Primary C++20 |

### 7.2 Required Clang Versions

| Version | C++ Standards | Notes |
|---------|---------------|-------|
| Clang 14 | C++17 | Minimum supported |
| Clang 15 | C++17, C++20 | |
| Clang 16 | C++17, C++20 | Primary C++20 |

### 7.3 Required MSVC Standards

| Standard | Notes |
|----------|-------|
| C++17 | Compatibility |
| C++20 | Primary |

---

## 8. Tooling Alignment

- Formatting, static-analysis, and lint configs must target C++20
- Clang-Format style file should set `Standard: C++20`
- Clang-Tidy checks should assume C++20 features available

---

## 9. Checklist for New Workflows

Before committing a new workflow:

- [ ] File named `.github/workflows/<component-name>.yml`
- [ ] Header block with directory structure documented
- [ ] All paths use `FAT_P/FAT_P/fat_p/` prefix
- [ ] `env:` block defines INCLUDE_DIR, TEST_SRC, BENCH_SRC
- [ ] `linux-gcc` job with GCC C++20 primary build
- [ ] `linux-clang` job with Clang C++20 primary build
- [ ] `windows-msvc` job with C++20 primary build
- [ ] `sanitizer-asan` job
- [ ] `sanitizer-ubsan` job
- [ ] `sanitizer-tsan` job
- [ ] `header-check` job
- [ ] `strict-warnings` job
- [ ] `benchmarks` job (weekly + manual trigger)
- [ ] Benchmark compilation flags are CPU-feature gated (avoid illegal instruction on runners)
- [ ] `ci-success` gate job
- [ ] Path triggers include the workflow file itself
- [ ] `schedule` trigger for weekly benchmarks
- [ ] **Component's `FATP_META.layer` is present and correct in header metadata**
- [ ] Tested locally with `act` or manual validation

---

## 10. Changelog

### v2.1 (January 2026)
- Updated layer integrity guidance to use `FATP_META.layer` as the single source of truth (no Doxygen `@layer` tags)
- Updated example workflows and scripts to extract layer from `FATP_META` blocks

### v2.0 (January 2026)
- Added Section 3: CI Goals with C++20 default / C++17 core gate
- Added Section 5: Project-wide verification workflows
  - `ci_core17.yml` — C++17 compile gate for Foundation/Containers/Concurrency
  - `ci_verify.yml` — Layer tag and dependency verification
  - `scripts/verify_layers.sh` — Layer verification script
- Updated Section 6: Component workflow template for C++20 primary
- Updated Section 7: Compiler matrix notes
- Added Section 8: Tooling alignment (C++20 target)
- Updated checklist for `FATP_META.layer` requirement

### v1.0 (December 2025)
- Initial CI Workflow Style Guide

---

*Fat-P CI Workflow Style Guide v2.0 -- January 2026*
