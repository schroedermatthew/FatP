cmake_minimum_required(VERSION 3.21)
# FATP_META:
#   meta_version: 1
#   component: BuildSystem
#   file_role: build_script
#   path: CMakeLists.txt
#   summary: "Top-level build configuration and options."
#   api_stability: in_work
#   layer: Infrastructure
#   related:
#     docs_search: ""
#     tests: []
#   hygiene:
#     pragma_once: false
#     include_guard: false
#     defines_total: 0
#     defines_unprefixed: 0
#     undefs_total: 0
#     includes_windows_h: false

# =========================================================
# Toolchain / vcpkg defaults (must be BEFORE project())
# =========================================================

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE
        "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file"
    )
endif()

if(NOT DEFINED VCPKG_MANIFEST_DIR)
    set(VCPKG_MANIFEST_DIR
        "${CMAKE_SOURCE_DIR}/tooling/vcpkg/benchmarks"
        CACHE PATH "vcpkg manifest directory"
    )
endif()

if(NOT DEFINED VCPKG_INSTALLED_DIR)
    set(VCPKG_INSTALLED_DIR
        "${CMAKE_SOURCE_DIR}/.vcpkg_installed"
        CACHE PATH "Persistent vcpkg install directory"
    )
endif()

# =========================================================
# Project
# =========================================================
project(FatP LANGUAGES CXX)

# =========================================================
# Hard architecture policy (x64 only)
# =========================================================
if(MSVC)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR
            "FatP does not support x86 builds. "
            "Configure with: cmake --preset default "
            "or: cmake -S . -B build -A x64"
        )
    endif()
endif()

# =========================================================
# Options
# =========================================================
option(FATP_BUILD_TESTS "Build FAT-P tests" ON)
option(FATP_BUILD_BENCHMARKS "Build FAT-P benchmarks" ON)

# =========================================================
# Core library (header-only)
# =========================================================
add_library(fatp INTERFACE)
target_include_directories(fatp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/fat_p>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty>
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/fat_p>
)

# =========================================================
# Testing support
# =========================================================
if(FATP_BUILD_TESTS)
    add_library(fatp_test INTERFACE)
    target_link_libraries(fatp_test INTERFACE fatp)
    enable_testing()
endif()

# =========================================================
# Load component helper functions (MUST be before use)
# =========================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FatPComponents.cmake)

# =========================================================
# Components (tests + benchmarks)
# =========================================================
file(GLOB COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/components/*")
foreach(COMPONENT_PATH ${COMPONENT_DIRS})
    if(IS_DIRECTORY "${COMPONENT_PATH}")
        get_filename_component(COMPONENT_NAME ${COMPONENT_PATH} NAME)
        if(COMPONENT_NAME STREQUAL "_shared")
            continue()
        endif()
        fatp_add_component_tests(${COMPONENT_NAME})
        fatp_add_component_benchmarks(${COMPONENT_NAME})
    endif()
endforeach()

message(STATUS "FAT-P configured successfully")
