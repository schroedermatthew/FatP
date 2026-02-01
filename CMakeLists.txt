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

# Suppress FindBoost deprecation warning (vcpkg provides its own cmake config)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

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
option(FATP_BENCH_USE_LOCAL_THIRDPARTY "Use local ThirdParty/ for benchmark dependencies (if present)" ON)
set(FATP_BENCH_EXTRA_INCLUDE_DIRS "" CACHE STRING "Additional include directories for benchmarks (semicolon-separated)")

# =========================================================
# Core library (header-only)
# =========================================================
add_library(fatp INTERFACE)
target_include_directories(fatp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/fat_p>
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/fat_p>
)

# =========================================================
# Benchmark-only dependencies (not exported/installed)
# =========================================================
add_library(fatp_bench_deps INTERFACE)

if(FATP_BUILD_BENCHMARKS)
    # vcpkg installed packages (primary source for competitor libraries)
    if(DEFINED VCPKG_INSTALLED_DIR AND EXISTS "${VCPKG_INSTALLED_DIR}")
        # Try triplet-specific path first, then fall back to common include
        if(DEFINED VCPKG_TARGET_TRIPLET AND EXISTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
            target_include_directories(fatp_bench_deps INTERFACE
                $<BUILD_INTERFACE:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include>
            )
            message(STATUS "FAT-P Benchmarks: Using vcpkg packages from ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
        elseif(EXISTS "${VCPKG_INSTALLED_DIR}/include")
            target_include_directories(fatp_bench_deps INTERFACE
                $<BUILD_INTERFACE:${VCPKG_INSTALLED_DIR}/include>
            )
            message(STATUS "FAT-P Benchmarks: Using vcpkg packages from ${VCPKG_INSTALLED_DIR}/include")
        else()
            # Auto-detect triplet directory
            file(GLOB _VCPKG_TRIPLET_DIRS "${VCPKG_INSTALLED_DIR}/*/include")
            list(LENGTH _VCPKG_TRIPLET_DIRS _VCPKG_TRIPLET_COUNT)
            if(_VCPKG_TRIPLET_COUNT GREATER 0)
                list(GET _VCPKG_TRIPLET_DIRS 0 _VCPKG_INCLUDE_DIR)
                target_include_directories(fatp_bench_deps INTERFACE
                    $<BUILD_INTERFACE:${_VCPKG_INCLUDE_DIR}>
                )
                message(STATUS "FAT-P Benchmarks: Using vcpkg packages from ${_VCPKG_INCLUDE_DIR}")
            else()
                message(STATUS "FAT-P Benchmarks: vcpkg installed dir exists but no packages found")
            endif()
        endif()
    else()
        message(STATUS "FAT-P Benchmarks: No vcpkg packages (VCPKG_INSTALLED_DIR not set or doesn't exist)")
    endif()

    # =========================================================
    # Find and link benchmark competitor libraries (non-header-only)
    # =========================================================
    
    # CRoaring (roaring bitmaps)
    find_package(roaring CONFIG QUIET)
    if(roaring_FOUND)
        target_link_libraries(fatp_bench_deps INTERFACE roaring::roaring)
        message(STATUS "FAT-P Benchmarks: Found CRoaring")
    endif()
    
    # LLVM (for BitVector, SmallVector comparisons)
    find_package(LLVM CONFIG QUIET)
    if(LLVM_FOUND)
        target_include_directories(fatp_bench_deps INTERFACE ${LLVM_INCLUDE_DIRS})
        # Link LLVMSupport which contains SmallVector, BitVector implementations
        # vcpkg uses different target naming conventions
        if(TARGET LLVMSupport)
            target_link_libraries(fatp_bench_deps INTERFACE LLVMSupport)
            message(STATUS "FAT-P Benchmarks: Found LLVM ${LLVM_VERSION}, linking LLVMSupport")
        elseif(TARGET LLVM::Support)
            target_link_libraries(fatp_bench_deps INTERFACE LLVM::Support)
            message(STATUS "FAT-P Benchmarks: Found LLVM ${LLVM_VERSION}, linking LLVM::Support")
        else()
            # Fallback: try to link all LLVM libs
            llvm_map_components_to_libnames(llvm_libs support)
            if(llvm_libs)
                target_link_libraries(fatp_bench_deps INTERFACE ${llvm_libs})
                message(STATUS "FAT-P Benchmarks: Found LLVM ${LLVM_VERSION}, linking ${llvm_libs}")
            else()
                message(STATUS "FAT-P Benchmarks: Found LLVM ${LLVM_VERSION} headers but could not find Support library - LLVM benchmarks may fail to link")
            endif()
        endif()
    endif()
    
    # Abseil (for flat_hash_map, InlinedVector comparisons)
    find_package(absl CONFIG QUIET)
    if(absl_FOUND)
        target_link_libraries(fatp_bench_deps INTERFACE 
            absl::flat_hash_map 
            absl::flat_hash_set
            absl::inlined_vector
            absl::hash
        )
        # Suppress DELAYLOAD warnings from Abseil's vcpkg build (LNK4199)
        if(MSVC)
            target_link_options(fatp_bench_deps INTERFACE /ignore:4199)
        endif()
        message(STATUS "FAT-P Benchmarks: Found Abseil")
    endif()
    
    # foonathan_memory (for memory pool comparisons)
    find_package(foonathan_memory CONFIG QUIET)
    if(foonathan_memory_FOUND)
        target_link_libraries(fatp_bench_deps INTERFACE foonathan_memory)
        message(STATUS "FAT-P Benchmarks: Found foonathan_memory")
    endif()
    
    # EASTL (for container comparisons)
    find_package(EASTL CONFIG QUIET)
    if(EASTL_FOUND)
        target_link_libraries(fatp_bench_deps INTERFACE EASTL)
        message(STATUS "FAT-P Benchmarks: Found EASTL")
    endif()
    
    # fmt (for string formatting comparisons)
    find_package(fmt CONFIG QUIET)
    if(fmt_FOUND)
        target_link_libraries(fatp_bench_deps INTERFACE fmt::fmt)
        message(STATUS "FAT-P Benchmarks: Found fmt")
    endif()
    
    # Boost (header-only components used, but find anyway for consistency)
    find_package(Boost QUIET)
    if(Boost_FOUND)
        target_include_directories(fatp_bench_deps INTERFACE ${Boost_INCLUDE_DIRS})
        message(STATUS "FAT-P Benchmarks: Found Boost ${Boost_VERSION}")
    endif()

    # Local ThirdParty/ for competitor headers NOT available in vcpkg (gitignored, developer convenience)
    set(_FATP_LOCAL_THIRDPARTY "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty")
    if(FATP_BENCH_USE_LOCAL_THIRDPARTY AND EXISTS "${_FATP_LOCAL_THIRDPARTY}")
        target_include_directories(fatp_bench_deps INTERFACE
            $<BUILD_INTERFACE:${_FATP_LOCAL_THIRDPARTY}>
        )
        message(STATUS "FAT-P Benchmarks: Using local ThirdParty/ for non-vcpkg competitor headers")
    endif()

    # User-supplied extra include directories
    if(FATP_BENCH_EXTRA_INCLUDE_DIRS)
        foreach(_EXTRA_DIR IN LISTS FATP_BENCH_EXTRA_INCLUDE_DIRS)
            if(EXISTS "${_EXTRA_DIR}")
                target_include_directories(fatp_bench_deps INTERFACE
                    $<BUILD_INTERFACE:${_EXTRA_DIR}>
                )
                message(STATUS "FAT-P Benchmarks: Added extra include dir: ${_EXTRA_DIR}")
            else()
                message(WARNING "FAT-P Benchmarks: Extra include dir not found: ${_EXTRA_DIR}")
            endif()
        endforeach()
    endif()
endif()

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
