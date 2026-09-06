# FATP_META:
#   meta_version: 1
#   component: BuildSystem
#   file_role: build_script
#   path: cmake/FatPComponents.cmake
#   namespace: global
#   layer: Testing
#   summary: Registers component test and benchmark translation units as CMake targets.
#   api_stability: in_work
#   hygiene:
#     pragma_once: false
#     include_guard: false
#     defines_total: 0
#     defines_unprefixed: 0
#     undefs_total: 0
#     includes_windows_h: false
#   generated:
#     by: codex
#     mode: manual

include_guard(GLOBAL)

if(FATP_BUILD_TESTS OR FATP_BUILD_BENCHMARKS)
    find_package(Threads REQUIRED)
endif()

function(fatp_configure_executable target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    target_link_libraries(${target_name} PRIVATE Threads::Threads)

    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /W4
            /bigobj
            /wd4127
            /wd4324
            /EHsc
            /permissive-
            /Zc:preprocessor
        )
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )

        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target_name} PRIVATE
                -Wno-gnu-zero-variadic-macro-arguments
            )
            if(WIN32)
                # Clang's integrated assembler writes classic COFF until 65,279
                # sections, while GNU ld treats symbol section numbers above
                # 32,767 as negative. Large unoptimized Tensor test objects hit
                # that gap. LLVM's linker accepts the complete section range;
                # keep all debug information and assertions in these builds.
                # -Wa,-mbig-obj is ignored by Clang's integrated assembler.
                target_link_options(${target_name} PRIVATE -fuse-ld=lld)
            endif()
        endif()
    endif()
endfunction()

function(fatp_add_component_tests component_name)
    if(NOT FATP_BUILD_TESTS)
        return()
    endif()

    set(test_dir "${CMAKE_SOURCE_DIR}/components/${component_name}/tests")
    if(NOT IS_DIRECTORY "${test_dir}")
        return()
    endif()

    file(GLOB test_sources CONFIGURE_DEPENDS "${test_dir}/test_*.cpp")
    foreach(test_source IN LISTS test_sources)
        get_filename_component(test_name "${test_source}" NAME_WE)

        # test_FatP.cpp is the legacy all-components orchestrator. The CMake
        # suite registers each component source as an independently runnable
        # test, so compiling that duplicate whole-suite link unit is unnecessary.
        if(test_name STREQUAL "test_FatP")
            continue()
        endif()

        add_executable(${test_name} "${test_source}")
        target_compile_definitions(${test_name} PRIVATE ENABLE_TEST_APPLICATION)
        target_link_libraries(${test_name} PRIVATE fatp_test)
        if(WIN32)
            target_link_libraries(${test_name} PRIVATE advapi32)
        endif()
        fatp_configure_executable(${test_name})

        # This regression must exercise result publication without optional NRVO.
        if(test_name STREQUAL "test_TensorMaterialization")
            if(MSVC)
                include(CheckCXXCompilerFlag)
                check_cxx_compiler_flag("/Zc:nrvo-" FATP_HAS_NO_OPTIONAL_NRVO)
                if(FATP_HAS_NO_OPTIONAL_NRVO)
                    target_compile_options(${test_name} PRIVATE /Zc:nrvo-)
                endif()
            else()
                target_compile_options(${test_name} PRIVATE -fno-elide-constructors)
            endif()
        endif()

        add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
        if(test_name STREQUAL "test_TensorExecution" OR test_name STREQUAL "test_TensorContractions" OR
           test_name STREQUAL "test_ThreadPool")
            set_tests_properties(${test_name} PROPERTIES TIMEOUT 120)
        endif()
    endforeach()
endfunction()

function(fatp_add_component_benchmarks component_name)
    if(NOT FATP_BUILD_BENCHMARKS)
        return()
    endif()

    set(benchmark_dir "${CMAKE_SOURCE_DIR}/components/${component_name}/benchmarks")
    if(NOT IS_DIRECTORY "${benchmark_dir}")
        return()
    endif()

    file(GLOB benchmark_sources CONFIGURE_DEPENDS "${benchmark_dir}/benchmark_*.cpp")
    foreach(benchmark_source IN LISTS benchmark_sources)
        get_filename_component(benchmark_name "${benchmark_source}" NAME_WE)

        add_executable(${benchmark_name} "${benchmark_source}")
        target_link_libraries(${benchmark_name} PRIVATE fatp fatp_bench_deps)
        if(WIN32)
            target_link_libraries(${benchmark_name} PRIVATE advapi32)
        endif()
        fatp_configure_executable(${benchmark_name})
        if(benchmark_name STREQUAL "benchmark_TensorMatmul" OR
           benchmark_name STREQUAL "benchmark_TensorExecution" OR
           benchmark_name STREQUAL "benchmark_TensorContractions")
            # Match the documented scalar-reference floating contract in CMake and manual CI builds.
            if(MSVC)
                target_compile_options(${benchmark_name} PRIVATE /fp:strict /volatile:iso)
            elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
                target_compile_options(${benchmark_name} PRIVATE -ffp-contract=off)
            endif()
        endif()
    endforeach()
endfunction()
