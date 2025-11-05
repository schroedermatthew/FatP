cmake_minimum_required(VERSION 3.15)
project(TensorMKL VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# Configuration Options
# ============================================================================

option(USE_MKL "Enable Intel MKL integration" ON)
option(BUILD_TESTS "Build test executables" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_BENCHMARKS "Build benchmark programs" ON)

# ============================================================================
# C++ Standard
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Compiler Flags
# ============================================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -march=native -DNDEBUG)
    endif()
elseif(MSVC)
    add_compile_options(/W4 /permissive-)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /DNDEBUG)
    endif()
endif()

# ============================================================================
# Find MKL (Optional)
# ============================================================================

if(USE_MKL)
    find_package(MKL CONFIG)
    
    if(MKL_FOUND)
        message(STATUS "Intel MKL found: ${MKL_VERSION}")
        message(STATUS "MKL Include: ${MKL_INCLUDE}")
        message(STATUS "MKL Libraries: ${MKL_LIBRARIES}")
        set(MKL_AVAILABLE TRUE)
    else()
        message(WARNING "Intel MKL not found, falling back to default implementation")
        set(MKL_AVAILABLE FALSE)
    endif()
else()
    set(MKL_AVAILABLE FALSE)
    message(STATUS "MKL integration disabled by user")
endif()

# ============================================================================
# Header-Only Library Target
# ============================================================================

add_library(tensor_mkl INTERFACE)
target_include_directories(tensor_mkl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

if(MKL_AVAILABLE)
    target_compile_definitions(tensor_mkl INTERFACE CPP_UTILITIES_USE_MKL=1)
    target_link_libraries(tensor_mkl INTERFACE MKL::MKL)
endif()

# ============================================================================
# Tests
# ============================================================================

if(BUILD_TESTS)
    enable_testing()
    
    # Test Tensor (Base)
    add_executable(test_tensor test_Tensor.cpp)
    target_link_libraries(test_tensor PRIVATE tensor_mkl)
    target_link_libraries(test_tensor PRIVATE pthread)
    add_test(NAME TensorTests COMMAND test_tensor)
    
    # Test TensorMKL
    add_executable(test_tensor_mkl test_TensorMKL.cpp)
    target_link_libraries(test_tensor_mkl PRIVATE tensor_mkl)
    target_link_libraries(test_tensor_mkl PRIVATE pthread)
    add_test(NAME TensorMKLTests COMMAND test_tensor_mkl)
    
    message(STATUS "Building tests: test_tensor, test_tensor_mkl")
endif()

# ============================================================================
# Examples
# ============================================================================

if(BUILD_EXAMPLES)
    add_executable(example_tensor_mkl example_tensor_mkl.cpp)
    target_link_libraries(example_tensor_mkl PRIVATE tensor_mkl)
    
    message(STATUS "Building example: example_tensor_mkl")
endif()

# ============================================================================
# Benchmarks
# ============================================================================

if(BUILD_BENCHMARKS)
    add_executable(benchmark_tensor benchmark_tensor.cpp)
    target_link_libraries(benchmark_tensor PRIVATE tensor_mkl)
    target_link_libraries(benchmark_tensor PRIVATE pthread)
    
    message(STATUS "Building benchmark: benchmark_tensor")
endif()

# ============================================================================
# Installation
# ============================================================================

install(
    FILES
        Tensor.h
        TensorMKL.h
        test_Utilities.h
    DESTINATION include/cpp_utilities
)

install(
    FILES
        MKL_INTEGRATION.txt
        TENSOR_README.txt
    DESTINATION share/doc/cpp_utilities
)

# ============================================================================
# Package Configuration
# ============================================================================

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/TensorMKLConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/TensorMKLConfig.cmake
    INSTALL_DESTINATION lib/cmake/TensorMKL
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/TensorMKLConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/TensorMKLConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/TensorMKLConfigVersion.cmake
    DESTINATION lib/cmake/TensorMKL
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "===========================================")
message(STATUS "  Tensor MKL Configuration Summary")
message(STATUS "===========================================")
message(STATUS "  C++ Standard:        ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:          ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler:            ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  MKL Integration:     ${MKL_AVAILABLE}")
if(MKL_AVAILABLE)
    message(STATUS "  MKL Version:         ${MKL_VERSION}")
endif()
message(STATUS "  Build Tests:         ${BUILD_TESTS}")
message(STATUS "  Build Examples:      ${BUILD_EXAMPLES}")
message(STATUS "  Build Benchmarks:    ${BUILD_BENCHMARKS}")
message(STATUS "===========================================")
message(STATUS "")

# ============================================================================
# Build Instructions
# ============================================================================

message(STATUS "Build Instructions:")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake ..")
message(STATUS "  cmake --build .")
message(STATUS "  ctest  # Run tests")
message(STATUS "")
