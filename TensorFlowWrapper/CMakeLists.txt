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
option(TF_WRAPPER_BUILD_TESTS "Build unit tests" ON)
option(TF_WRAPPER_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(TF_WRAPPER_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(TF_WRAPPER_TF_STUB "Build with a minimal TensorFlow C API stub (no TensorFlow dependency)" OFF)

# ============================================================================
# C++20 Requirement
# ============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# TensorFlow dependency (real TF or stub)
# ============================================================================

set(TF_WRAPPER_TF_INCLUDE_DIR "")
set(TF_WRAPPER_TF_LIBRARIES "")
set(TF_WRAPPER_TEST_TF_LIBRARIES "")

if(TF_WRAPPER_TF_STUB)
    # ------------------------------------------------------------------------
    # Stub mode (B2): no TensorFlow install required.
    # - Provide a minimal header shim at third_party/tf_stub/tensorflow/c/c_api.h
    # - Link tests/examples against a tiny implementation of the referenced symbols.
    # ------------------------------------------------------------------------
    set(TF_WRAPPER_TF_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tf_stub")

    add_library(tf_c_stub STATIC
        third_party/tf_stub/tf_c_stub.cpp)
    add_library(tf::c_stub ALIAS tf_c_stub)

    target_include_directories(tf_c_stub PUBLIC
        ${TF_WRAPPER_TF_INCLUDE_DIR})

    target_compile_features(tf_c_stub PUBLIC cxx_std_20)

    target_compile_definitions(tf_c_stub PUBLIC TF_WRAPPER_TF_STUB_ENABLED=1)

    # Header-only wrapper should not gain an installation-time dependency on
    # the stub library. Only the test/executable targets link against it.
    set(TF_WRAPPER_TEST_TF_LIBRARIES tf_c_stub)
    message(STATUS "TensorFlow: using stub (TF_WRAPPER_TF_STUB=ON)")
else()
    # ------------------------------------------------------------------------
    # Real TensorFlow C library discovery
    # ------------------------------------------------------------------------
    if(NOT DEFINED TF_ROOT)
        if(DEFINED ENV{TF_ROOT})
            set(TF_ROOT "$ENV{TF_ROOT}")
        elseif(DEFINED ENV{TensorFlow_ROOT})
            set(TF_ROOT "$ENV{TensorFlow_ROOT}")
        else()
            set(TF_ROOT "/usr/local")
        endif()
        set(TF_ROOT "${TF_ROOT}" CACHE PATH "TensorFlow installation root")
    endif()

    find_path(TF_INCLUDE_DIR
        NAMES tensorflow/c/c_api.h
        HINTS "${TF_ROOT}"
        PATH_SUFFIXES include
        PATHS
            "/usr/include"
            "/usr/local/include"
            "$ENV{HOME}/.local/include"
        DOC "TensorFlow C API headers")

    find_library(TF_LIBRARY
        NAMES tensorflow
        HINTS "${TF_ROOT}"
        PATH_SUFFIXES lib lib64
        PATHS
            "/usr/lib"
            "/usr/lib64"
            "/usr/local/lib"
            "/usr/local/lib64"
            "$ENV{HOME}/.local/lib"
        DOC "TensorFlow C library")

    find_library(TF_FRAMEWORK_LIBRARY
        NAMES tensorflow_framework
        PATHS
            "${TF_ROOT}"
            "${TF_ROOT}/lib"
            "${TF_ROOT}/lib64"
            "/usr/lib"
            "/usr/lib64"
            "/usr/local/lib"
            "/usr/local/lib64"
            "$ENV{HOME}/.local/lib"
        DOC "TensorFlow framework library")

    if(WIN32)
        find_file(TF_RUNTIME_DLL
            NAMES tensorflow.dll
            HINTS "${TF_ROOT}"
            PATH_SUFFIXES bin lib
            DOC "TensorFlow runtime DLL")
    endif()

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

    set(TF_WRAPPER_TF_INCLUDE_DIR "${TF_INCLUDE_DIR}")
    set(TF_WRAPPER_TF_LIBRARIES "${TF_LIBRARY}")
    set(TF_WRAPPER_TEST_TF_LIBRARIES "${TF_LIBRARY}")

    message(STATUS "Found TensorFlow headers: ${TF_INCLUDE_DIR}")
    message(STATUS "Found TensorFlow library: ${TF_LIBRARY}")
    if(TF_RUNTIME_DLL)
        message(STATUS "Found TensorFlow runtime DLL: ${TF_RUNTIME_DLL}")
    endif()

    # Also link TensorFlow framework if found (needed for some installations)
    if(TF_FRAMEWORK_LIBRARY)
        list(APPEND TF_WRAPPER_TF_LIBRARIES ${TF_FRAMEWORK_LIBRARY})
        message(STATUS "Found TensorFlow framework: ${TF_FRAMEWORK_LIBRARY}")
    endif()
endif()

# ============================================================================
# Header-Only Library Target
# ============================================================================

add_library(tf_wrapper INTERFACE)
add_library(tf::wrapper ALIAS tf_wrapper)

target_include_directories(tf_wrapper INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    )

# Treat TensorFlow headers as SYSTEM to avoid propagating third-party warnings
target_include_directories(tf_wrapper SYSTEM INTERFACE
    $<BUILD_INTERFACE:${TF_WRAPPER_TF_INCLUDE_DIR}>)

if(TF_WRAPPER_TF_LIBRARIES)
    target_link_libraries(tf_wrapper INTERFACE ${TF_WRAPPER_TF_LIBRARIES})
endif()

# Also link TensorFlow framework if found (needed for some installations)
if(TF_FRAMEWORK_LIBRARY AND NOT TF_WRAPPER_TF_STUB)
    target_link_libraries(tf_wrapper INTERFACE ${TF_FRAMEWORK_LIBRARY})
    message(STATUS "Found TensorFlow framework: ${TF_FRAMEWORK_LIBRARY}")
endif()

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

        if(TF_WRAPPER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4)

        if(TF_WRAPPER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    endif()
    
    if(TF_WRAPPER_ENABLE_ASAN AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address)
    endif()
endfunction()

function(tf_wrapper_setup_runtime target)
    # Windows: copy runtime DLL next to the executable so it can run in CI
    if(WIN32 AND TF_RUNTIME_DLL)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${TF_RUNTIME_DLL}"
                $<TARGET_FILE_DIR:${target}>
            VERBATIM)
    endif()

    # Unix-like: prefer explicit build rpath for non-system TF installs
    if(APPLE OR UNIX)
        get_filename_component(_tf_lib_dir "${TF_LIBRARY}" DIRECTORY)
        if(_tf_lib_dir)
            set_property(TARGET ${target} APPEND PROPERTY BUILD_RPATH "${_tf_lib_dir}")
        endif()

        if(TF_FRAMEWORK_LIBRARY)
            get_filename_component(_tf_fw_dir "${TF_FRAMEWORK_LIBRARY}" DIRECTORY)
            if(_tf_fw_dir)
                set_property(TARGET ${target} APPEND PROPERTY BUILD_RPATH "${_tf_fw_dir}")
            endif()
        endif()
    endif()
endfunction()

# ============================================================================
# Example
# ============================================================================

if(TF_WRAPPER_BUILD_EXAMPLES)
    add_executable(tf_example example/main.cpp)
    target_link_libraries(tf_example PRIVATE tf::wrapper)
    add_strict_warnings(tf_example)
    tf_wrapper_setup_runtime(tf_example)
endif()

# ============================================================================
# Tests
# ============================================================================

if(TF_WRAPPER_BUILD_TESTS)
    enable_testing()

    # Main unit tests
    add_executable(tf_tests tests/test_main.cpp)
    target_link_libraries(tf_tests PRIVATE tf::wrapper ${TF_WRAPPER_TEST_TF_LIBRARIES})
    if(TF_WRAPPER_TF_STUB)
        target_compile_definitions(tf_tests PRIVATE TF_WRAPPER_TF_STUB_ENABLED=1)
    endif()
    add_strict_warnings(tf_tests)
    tf_wrapper_setup_runtime(tf_tests)
    add_test(NAME tf_tests COMMAND tf_tests)
    set_tests_properties(tf_tests PROPERTIES LABELS "unit")

    # Edge case and stress tests
    add_executable(tf_edge_tests tests/test_edge_cases.cpp)
    target_link_libraries(tf_edge_tests PRIVATE tf::wrapper ${TF_WRAPPER_TEST_TF_LIBRARIES})
    if(TF_WRAPPER_TF_STUB)
        target_compile_definitions(tf_edge_tests PRIVATE TF_WRAPPER_TF_STUB_ENABLED=1)
    endif()
    add_strict_warnings(tf_edge_tests)
    tf_wrapper_setup_runtime(tf_edge_tests)
    add_test(NAME tf_edge_tests COMMAND tf_edge_tests)
    set_tests_properties(tf_edge_tests PROPERTIES LABELS "unit")

    # Stress tests run separately with longer timeout
    add_test(NAME tf_stress_tests COMMAND tf_edge_tests --stress)
    set_tests_properties(tf_stress_tests PROPERTIES TIMEOUT 120)
    set_tests_properties(tf_stress_tests PROPERTIES LABELS "unit")

    # Integration tests (require real TensorFlow)
    if(NOT TF_WRAPPER_TF_STUB)
        add_executable(tf_integration_tests tests/test_integration.cpp)
        target_link_libraries(tf_integration_tests PRIVATE tf::wrapper ${TF_WRAPPER_TEST_TF_LIBRARIES})
        add_strict_warnings(tf_integration_tests)
        tf_wrapper_setup_runtime(tf_integration_tests)
        add_test(NAME tf_integration_tests COMMAND tf_integration_tests)
        set_tests_properties(tf_integration_tests PROPERTIES LABELS "integration")
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
message(STATUS "  Warnings->Err:  ${TF_WRAPPER_WARNINGS_AS_ERRORS}")
message(STATUS "")
