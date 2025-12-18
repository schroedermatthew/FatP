# CMake Project Structure Sketch

A CMake configuration for a multi-module C++ project with the following characteristics:

## Overview

### Dependencies
- **Boost**: Unit test framework and property testing
- **MKL**: Intel Math Kernel Library
- **OpenMP**: Parallel computing

### Project Structure
| Component | Type | Description |
|-----------|------|-------------|
| **NC** | Executable | Main application |
| **NCL** | Static Library | Core code linked by NC |
| **B** | Static Library | Submodule linking all dependencies |
| **FP** | Header-Only | Shared utilities included by NC and B |

### Key Relationships
- NC links NCL and OpenMP
- NCL links B and FP
- B links MKL, OpenMP, and FP
- Every module has unit tests requiring appropriate dependencies

---

## Directory Layout

```
project/
├── CMakeLists.txt              # Root CMake configuration
├── cmake/
│   └── FindMKL.cmake           # Optional: custom MKL finder
├── FP/
│   ├── CMakeLists.txt
│   ├── include/FP/
│   └── tests/
├── B/
│   ├── CMakeLists.txt
│   ├── include/B/
│   ├── src/
│   └── tests/
├── NCL/
│   ├── CMakeLists.txt
│   ├── include/NCL/
│   ├── src/
│   └── tests/
└── NC/
    ├── CMakeLists.txt
    └── src/
```

---

## Dependency Graph

```
NC (exe)
 └── NCL (lib)
      ├── B (lib) ─────┬── MKL::MKL
      │                ├── OpenMP::OpenMP_CXX  
      │                └── FP (interface)
      └── FP (interface)

B_tests (exe)
 └── B ─── [all B's PUBLIC deps]

NCL_tests (exe)
 └── NCL ─── B ─── [all deps]

FP_tests (exe)
 └── FP ─── Boost::unit_test_framework
```

---

## CMake Files

### Root `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# OPTIONS
# ============================================================
option(BUILD_TESTS "Build unit tests" ON)

# ============================================================
# DEPENDENCIES
# ============================================================

# OpenMP
find_package(OpenMP REQUIRED)

# MKL - Use Intel's config or custom finder
set(MKL_INTERFACE lp64)    # or ilp64
set(MKL_THREADING openmp)  # important: match OpenMP
find_package(MKL CONFIG REQUIRED)
# Creates target: MKL::MKL

# Boost
find_package(Boost REQUIRED COMPONENTS unit_test_framework)
# Creates targets: Boost::unit_test_framework, Boost::headers

# ============================================================
# TESTING
# ============================================================
if(BUILD_TESTS)
    enable_testing()
endif()

# ============================================================
# SUBDIRECTORIES (order matters for dependencies)
# ============================================================
add_subdirectory(FP)    # Header-only, no deps
add_subdirectory(B)     # Depends on FP, MKL, OpenMP
add_subdirectory(NCL)   # Depends on FP, B
add_subdirectory(NC)    # Main executable
```

---

### `FP/CMakeLists.txt` (Header-Only Library)

```cmake
# ============================================================
# FP - Header-only library
# ============================================================
add_library(FP INTERFACE)
add_library(MyProject::FP ALIAS FP)

target_include_directories(FP
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# ============================================================
# FP TESTS (no external dependencies)
# ============================================================
if(BUILD_TESTS)
    add_executable(FP_tests
        tests/test_main.cpp
        tests/test_fp_feature1.cpp
        # ... more test sources
    )
    
    target_link_libraries(FP_tests
        PRIVATE
            FP
            Boost::unit_test_framework
    )
    
    add_test(NAME FP_tests COMMAND FP_tests)
endif()
```

---

### `B/CMakeLists.txt`

```cmake
# ============================================================
# B - Library with all heavy dependencies
# ============================================================
add_library(B STATIC)  # or SHARED
add_library(MyProject::B ALIAS B)

target_sources(B
    PRIVATE
        src/b_impl1.cpp
        src/b_impl2.cpp
        # ... more sources
)

target_include_directories(B
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# KEY: PUBLIC propagates these to anything linking B
target_link_libraries(B
    PUBLIC
        MyProject::FP           # Header-only, used in B's public headers
        MKL::MKL                # If MKL types in public headers
        OpenMP::OpenMP_CXX      # OpenMP pragmas need this propagated
    PRIVATE
        # Put here if only used in .cpp files
)

# MKL often needs compile definitions
target_compile_definitions(B
    PUBLIC
        MKL_ILP64  # if using ilp64, else remove
)

# ============================================================
# B TESTS
# ============================================================
if(BUILD_TESTS)
    add_executable(B_tests
        tests/test_main.cpp
        tests/test_b_feature1.cpp
        tests/test_b_feature2.cpp
    )
    
    # Link B (PUBLIC deps come along transitively)
    target_link_libraries(B_tests
        PRIVATE
            B                           # Brings MKL, OpenMP, FP transitively
            Boost::unit_test_framework
    )
    
    # If tests need internal headers
    target_include_directories(B_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src  # internal headers if any
    )
    
    add_test(NAME B_tests COMMAND B_tests)
endif()
```

---

### `NCL/CMakeLists.txt`

```cmake
# ============================================================
# NCL - Main code as library
# ============================================================
add_library(NCL STATIC)
add_library(MyProject::NCL ALIAS NCL)

target_sources(NCL
    PRIVATE
        src/ncl_core.cpp
        src/ncl_algorithms.cpp
        # ... more sources
)

target_include_directories(NCL
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# B is PUBLIC if NCL headers expose B types
target_link_libraries(NCL
    PUBLIC
        MyProject::B    # Transitively brings MKL, OpenMP, FP
        MyProject::FP
)

# ============================================================
# NCL TESTS
# ============================================================
if(BUILD_TESTS)
    add_executable(NCL_tests
        tests/test_main.cpp
        tests/test_ncl_core.cpp
        tests/test_ncl_algorithms.cpp
    )
    
    target_link_libraries(NCL_tests
        PRIVATE
            NCL                         # Brings B, MKL, OpenMP, FP transitively
            Boost::unit_test_framework
    )
    
    add_test(NAME NCL_tests COMMAND NCL_tests)
endif()
```

---

### `NC/CMakeLists.txt` (Main Executable)

```cmake
# ============================================================
# NC - Main Executable
# ============================================================
add_executable(NC
    src/main.cpp
)

target_link_libraries(NC
    PRIVATE
        MyProject::NCL      # Everything comes transitively
        OpenMP::OpenMP_CXX  # Explicit if main.cpp uses OpenMP directly
)
```

---

## Common Linker Error Fixes

### 1. Missing Symbols from MKL

MKL often requires explicit threading and math libraries on Linux:

```cmake
# Ensure MKL links its dependencies
target_link_libraries(B
    PUBLIC
        MKL::MKL
        $<$<CXX_COMPILER_ID:GNU>:pthread>
        $<$<CXX_COMPILER_ID:GNU>:m>
        $<$<CXX_COMPILER_ID:GNU>:dl>
)
```

### 2. OpenMP Not Propagating

If headers contain `#pragma omp` directives, OpenMP must be `PUBLIC`:

```cmake
target_link_libraries(B PUBLIC OpenMP::OpenMP_CXX)
```

### 3. Windows DLL Export Issues

When using `SHARED` libraries on Windows:

```cmake
# In B/CMakeLists.txt
include(GenerateExportHeader)
generate_export_header(B)
target_include_directories(B PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
```

### 4. Boost Test Configuration

Ensure only one translation unit defines `BOOST_TEST_MODULE`:

```cpp
// tests/test_main.cpp
#define BOOST_TEST_MODULE MyTests
#include <boost/test/unit_test.hpp>

// Other test files should NOT define BOOST_TEST_MODULE
// tests/test_feature.cpp
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(my_test) {
    // ...
}
```

---

## Key Concepts

### PUBLIC vs PRIVATE Dependencies

| Specifier | When to Use |
|-----------|-------------|
| **PUBLIC** | Dependency headers are exposed in your public headers |
| **PRIVATE** | Dependency is only used in `.cpp` implementation files |
| **INTERFACE** | For header-only libraries (no compiled sources) |

Using `PUBLIC` correctly ensures transitive linking works for test executables.

### Generator Expressions

```cmake
$<BUILD_INTERFACE:...>    # Used when building
$<INSTALL_INTERFACE:...>  # Used when installed
$<$<CXX_COMPILER_ID:GNU>:...>  # Conditional on compiler
```

---

## Build Commands

```bash
# Configure
cmake -B build -DBUILD_TESTS=ON

# Build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure

# Build specific target
cmake --build build --target B_tests
```

---

## Troubleshooting Checklist

- [ ] All `PUBLIC` dependencies are correctly marked for libraries with exposed headers
- [ ] MKL threading model matches OpenMP configuration
- [ ] Boost test module defined in exactly one `.cpp` file per test executable
- [ ] Include directories use generator expressions for build/install separation
- [ ] Subdirectories added in dependency order in root CMakeLists.txt
- [ ] Test executables link the library target, not individual source files
