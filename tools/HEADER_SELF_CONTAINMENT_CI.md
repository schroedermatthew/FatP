# Header Self-Containment CI Tools

This document describes the tools provided for verifying that all Fat-P headers are **self-contained** — meaning each header can be included standalone without requiring other headers to be included first.

## Why Self-Containment Matters

A self-contained header:
- Includes all its own dependencies
- Can be included in any order
- Doesn't break when include order changes
- Makes the library easier to use and maintain

## Tools Overview

| Tool | Use Case |
|------|----------|
| `check_header_self_containment.py` | Local development, CI pipelines, detailed diagnostics |
| `check_header_self_containment.sh` | Quick checks, minimal dependencies, shell scripting |
| `CheckHeaderSelfContainment.cmake` | CMake-based projects, integration with `ctest` |
| `.github/workflows/header-hygiene.yml` | Automated CI on GitHub (push/PR triggers) |

---

## 1. Python Script

### Location
```
scripts/check_header_self_containment.py
```

### Requirements
- Python 3.6+
- C++ compiler (g++, clang++, or cl)

### Basic Usage
```bash
# From repository root
python scripts/check_header_self_containment.py --include-dir fat_p
```

### Options
| Option | Short | Default | Description |
|--------|-------|---------|-------------|
| `--compiler` | `-c` | `g++` | C++ compiler to use |
| `--std` | `-s` | `c++20` | C++ standard |
| `--include-dir` | `-I` | `fat_p` | Directory containing headers |
| `--verbose` | `-v` | off | Print status for each header |
| `--continue-on-error` | | off | Check all headers even if some fail |

### Examples

**Quick check with GCC:**
```bash
python check_header_self_containment.py -I fat_p
```

**Verbose output with Clang:**
```bash
python check_header_self_containment.py -c clang++ -I fat_p -v
```

**Check all headers, report all failures:**
```bash
python check_header_self_containment.py -I fat_p -v --continue-on-error
```

**Use C++17 standard:**
```bash
python check_header_self_containment.py -I fat_p -s c++17
```

### Output Example
```
Checking header self-containment
  Compiler: g++
  Standard: c++20
  Include dir: fat_p

Found 89 headers to check

Checking AlignedVector.h...                       OK
Checking AsyncOperations.h...                     OK
Checking CheckedArithmetic_IntSimd_AVX2.h...      FAILED
  Error: fatal error: 'CheckedArithmeticPolicies.h' file not found

============================================================
Results: 88/89 headers passed

FAILED HEADERS:
  - CheckedArithmetic_IntSimd_AVX2.h
      <stdin>:1:10: fatal error: 'CheckedArithmeticPolicies.h'

FAILED: Not all headers are self-contained
```

### Exit Codes
- `0` — All headers passed
- `1` — One or more headers failed

---

## 2. Bash Script

### Location
```
scripts/check_header_self_containment.sh
```

### Requirements
- Bash 4.0+
- C++ compiler (g++, clang++)

### Basic Usage
```bash
# Make executable (once)
chmod +x scripts/check_header_self_containment.sh

# Run
./scripts/check_header_self_containment.sh -I fat_p
```

### Options
| Option | Short | Default | Description |
|--------|-------|---------|-------------|
| `--compiler` | `-c` | `g++` | C++ compiler to use |
| `--std` | `-s` | `c++20` | C++ standard |
| `--include-dir` | `-I` | `fat_p` | Directory containing headers |
| `--verbose` | `-v` | off | Print status for each header |
| `--help` | `-h` | | Show help message |

### Examples

**Quick check:**
```bash
./check_header_self_containment.sh -I fat_p
```

**Verbose with Clang:**
```bash
./check_header_self_containment.sh -c clang++ -I fat_p -v
```

### Exit Codes
- `0` — All headers passed
- `1` — One or more headers failed

---

## 3. CMake Module

### Location
```
cmake/CheckHeaderSelfContainment.cmake
```

### Integration

Add to your `CMakeLists.txt`:

```cmake
# Include the module
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CheckHeaderSelfContainment)

# Add header self-containment tests
add_header_self_containment_tests(
    TARGET header_tests
    HEADERS_DIR ${CMAKE_SOURCE_DIR}/fat_p
    CXX_STANDARD 20
)
```

### Function Signature
```cmake
add_header_self_containment_tests(
    TARGET <target_name>           # Name for the test target
    HEADERS_DIR <directory>        # Directory containing headers
    CXX_STANDARD <standard>        # C++ standard (default: 20)
    EXCLUDE <header1> <header2>    # Headers to skip (optional)
)
```

### Running Tests

```bash
# Configure
cmake -B build -S .

# Run all header tests
ctest --test-dir build -R header_self_contained

# Run specific header test
ctest --test-dir build -R header_self_contained_SimdVector_h

# Verbose output
ctest --test-dir build -R header_self_contained -V
```

### Example with Exclusions

```cmake
add_header_self_containment_tests(
    TARGET header_tests
    HEADERS_DIR ${CMAKE_SOURCE_DIR}/fat_p
    CXX_STANDARD 20
    EXCLUDE
        internal_detail.h    # Internal header, not self-contained by design
        platform_specific.h  # Requires platform headers
)
```

---

## 4. GitHub Actions Workflow

### Location
```
.github/workflows/header-hygiene.yml
```

### Automatic Triggers

The workflow runs automatically on:
- **Push** to `main` or `develop` branches
- **Pull requests** targeting `main` or `develop`

Only when these paths change:
- `fat_p/**`
- `tests/IncludeAllFatPHeaders.h`
- `.github/workflows/header-hygiene.yml`

### Jobs

| Job | Description | Platforms |
|-----|-------------|-----------|
| `header-self-containment` | Each header compiles standalone | Ubuntu (GCC, Clang), macOS (Clang) |
| `include-all-headers` | `IncludeAllFatPHeaders.h` compiles | Ubuntu (GCC, Clang), macOS (Clang) |
| `windows-header-check` | Headers compile with MSVC | Windows |

### Manual Trigger

You can also trigger the workflow manually from the GitHub Actions tab.

### Viewing Results

1. Go to **Actions** tab in GitHub
2. Click on the workflow run
3. Expand failed jobs to see which headers failed
4. Error messages show the missing includes

---

## Fixing Self-Containment Issues

When a header fails, the error message typically shows:
```
fatal error: 'SomeHeader.h' file not found
```

### Fix Process

1. **Identify the missing dependency** from the error message

2. **Add the include** to the failing header:
   ```cpp
   // In the header that failed
   #include "SomeHeader.h"  // Add this
   ```

3. **Place includes correctly:**
   - Standard library includes first (`<vector>`, `<string>`)
   - Then Fat-P includes (`"enforce.h"`, `"FatPTypeTraits.h"`)

4. **Re-run the check** to verify the fix

### Example Fix

**Error:**
```
CheckedArithmetic_IntSimd_AVX2.h:166: error: 'ReturnExpectedPolicy' not declared
```

**Fix:** Add to `CheckedArithmetic_IntSimd_Common.h`:
```cpp
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "CheckedArithmeticPolicies.h"  // ADD THIS LINE
```

---

## Best Practices

### For Header Authors

1. **Always include what you use** — don't rely on transitive includes
2. **Use forward declarations** when full definition isn't needed
3. **Test standalone compilation** before committing
4. **Run the CI locally** before pushing

### For CI Integration

1. **Run on every PR** — catch issues before merge
2. **Test multiple compilers** — GCC, Clang, MSVC have different behaviors
3. **Use `--continue-on-error`** — see all failures at once
4. **Keep the exclude list minimal** — most headers should be self-contained

---

## Troubleshooting

### "Compiler not found"

```bash
# Check compiler is installed
which g++
which clang++

# Install if missing (Ubuntu)
sudo apt-get install g++
sudo apt-get install clang
```

### "Include directory not found"

Ensure you're running from the repository root:
```bash
cd /path/to/fat-p-repo
./scripts/check_header_self_containment.sh -I fat_p
```

### "All headers fail"

Check the include path is correct:
```bash
ls fat_p/*.h  # Should list headers
```

### Windows-specific Issues

On Windows with MSVC, use the CMake approach or run from a "Developer Command Prompt":
```cmd
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
cl /std:c++20 /Zs /I fat_p fat_p\SimdVector.h
```

---

## Summary

| Scenario | Recommended Tool |
|----------|------------------|
| Quick local check | `check_header_self_containment.sh` |
| Detailed diagnostics | `check_header_self_containment.py` |
| CMake project integration | `CheckHeaderSelfContainment.cmake` |
| Automated CI | `.github/workflows/header-hygiene.yml` |
