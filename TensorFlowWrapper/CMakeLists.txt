# CMakeLists.txt
# TensorFlow C++20 Wrapper - Merged Implementation
#
# This is the "best of both worlds" implementation combining:
# - ChatGPT's correctness-focused approach
# - Claude's comprehensive feature set

cmake_minimum_required(VERSION 3.16)
project(tf_cpp20_wrapper
    VERSION 2.0.0
    LANGUAGES CXX
    DESCRIPTION "Modern C++20 wrapper for TensorFlow C API - Merged Implementation")

# ============================================================================
# Options
# ============================================================================

option(TF_WRAPPER_BUILD_EXAMPLES "Build example programs" ON)
option(TF_WRAPPER_BUILD_TESTS "Build unit tests" OFF)
option(TF_WRAPPER_ENABLE_ASAN "Enable AddressSanitizer" OFF)

# ============================================================================
# C++20 Requirement
# ============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Find TensorFlow C Library
# ============================================================================

if(NOT DEFINED TF_ROOT)
    set(TF_ROOT "/usr/local" CACHE PATH "TensorFlow installation root")
endif()

find_path(TF_INCLUDE_DIR
    NAMES tensorflow/c/c_api.h
    PATHS
        "${TF_ROOT}/include"
        "/usr/include"
        "/usr/local/include"
        "$ENV{HOME}/.local/include"
    DOC "TensorFlow C API headers")

find_library(TF_LIBRARY
    NAMES tensorflow
    PATHS
        "${TF_ROOT}/lib"
        "${TF_ROOT}/lib64"
        "/usr/lib"
        "/usr/lib64"
        "/usr/local/lib"
        "/usr/local/lib64"
        "$ENV{HOME}/.local/lib"
    DOC "TensorFlow C library")

if(NOT TF_INCLUDE_DIR)
    message(FATAL_ERROR
        "TensorFlow C headers not found.\n"
        "Set TF_ROOT to the TensorFlow installation directory, or:\n"
        "  - Ubuntu: apt install libtensorflow-dev\n"
        "  - macOS:  brew install tensorflow\n"
        "  - Manual: https://www.tensorflow.org/install/lang_c")
endif()

if(NOT TF_LIBRARY)
    message(FATAL_ERROR
        "TensorFlow C library not found.\n"
        "Set TF_ROOT to the TensorFlow installation directory.")
endif()

message(STATUS "Found TensorFlow headers: ${TF_INCLUDE_DIR}")
message(STATUS "Found TensorFlow library: ${TF_LIBRARY}")

# ============================================================================
# Header-Only Library Target
# ============================================================================

add_library(tf_wrapper INTERFACE)
add_library(tf::wrapper ALIAS tf_wrapper)

target_include_directories(tf_wrapper INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${TF_INCLUDE_DIR})

target_link_libraries(tf_wrapper INTERFACE ${TF_LIBRARY})

target_compile_features(tf_wrapper INTERFACE cxx_std_20)

# Platform-specific
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(tf_wrapper INTERFACE pthread)
endif()

# ============================================================================
# Compiler Warnings Helper
# ============================================================================

function(add_strict_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wconversion -Wshadow -Wcast-qual
            -Wnon-virtual-dtor -Wold-style-cast
            -Woverloaded-virtual -Wsign-conversion)
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    endif()
    
    if(TF_WRAPPER_ENABLE_ASAN AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address)
    endif()
endfunction()

# ============================================================================
# Example
# ============================================================================

if(TF_WRAPPER_BUILD_EXAMPLES)
    add_executable(tf_example example/main.cpp)
    target_link_libraries(tf_example PRIVATE tf::wrapper)
    add_strict_warnings(tf_example)
endif()

# ============================================================================
# Tests
# ============================================================================

if(TF_WRAPPER_BUILD_TESTS)
    enable_testing()
    
    find_package(Catch2 3 QUIET)
    
    if(Catch2_FOUND)
        add_executable(tf_tests tests/test_main.cpp)
        target_link_libraries(tf_tests PRIVATE tf::wrapper Catch2::Catch2WithMain)
        add_strict_warnings(tf_tests)
        
        include(Catch)
        catch_discover_tests(tf_tests)
    else()
        message(WARNING
            "Catch2 not found, tests will not be built.\n"
            "Install with: apt install catch2 (Ubuntu 22.04+)")
    endif()
endif()

# ============================================================================
# Installation
# ============================================================================

include(GNUInstallDirs)

install(TARGETS tf_wrapper
    EXPORT tf_wrapper_targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY include/tf
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT tf_wrapper_targets
    FILE tf_wrapper-targets.cmake
    NAMESPACE tf::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tf_wrapper)

# Package config
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tf_wrapper-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/tf_wrapper-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tf_wrapper)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/tf_wrapper-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/tf_wrapper-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/tf_wrapper-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tf_wrapper)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "TensorFlow C++20 Wrapper - Configuration Summary")
message(STATUS "================================================")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Examples: ${TF_WRAPPER_BUILD_EXAMPLES}")
message(STATUS "  Build Tests:    ${TF_WRAPPER_BUILD_TESTS}")
message(STATUS "  ASAN:           ${TF_WRAPPER_ENABLE_ASAN}")
message(STATUS "")
