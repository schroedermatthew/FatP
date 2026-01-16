# CheckHeaderSelfContainment.cmake
#
# CMake module to verify Fat-P headers are self-contained.
# Adds a test target that compiles each header standalone.
#
# Usage in CMakeLists.txt:
#     include(CheckHeaderSelfContainment)
#     add_header_self_containment_tests(
#         TARGET header_self_containment_tests
#         HEADERS_DIR ${CMAKE_SOURCE_DIR}/fat_p
#         CXX_STANDARD 20
#     )
#
# Then run: ctest -R header_self_containment

function(add_header_self_containment_tests)
    cmake_parse_arguments(
        ARG
        ""                          # Options
        "TARGET;HEADERS_DIR;CXX_STANDARD"  # One-value arguments
        "EXCLUDE"                   # Multi-value arguments
        ${ARGN}
    )
    
    # Defaults
    if(NOT ARG_TARGET)
        set(ARG_TARGET "header_self_containment")
    endif()
    if(NOT ARG_CXX_STANDARD)
        set(ARG_CXX_STANDARD 20)
    endif()
    if(NOT ARG_HEADERS_DIR)
        message(FATAL_ERROR "HEADERS_DIR must be specified")
    endif()
    
    # Find all headers
    file(GLOB HEADERS 
        "${ARG_HEADERS_DIR}/*.h"
        "${ARG_HEADERS_DIR}/*.hpp"
    )
    
    # Remove excluded headers
    if(ARG_EXCLUDE)
        foreach(excl ${ARG_EXCLUDE})
            list(FILTER HEADERS EXCLUDE REGEX ".*/${excl}$")
        endforeach()
    endif()
    
    list(LENGTH HEADERS HEADER_COUNT)
    message(STATUS "Adding self-containment tests for ${HEADER_COUNT} headers")
    
    # Create a test for each header
    foreach(header ${HEADERS})
        get_filename_component(header_name ${header} NAME)
        string(REPLACE "." "_" test_name "header_self_contained_${header_name}")
        
        # Create a source file that includes just this header
        set(test_source_dir "${CMAKE_BINARY_DIR}/header_tests")
        file(MAKE_DIRECTORY ${test_source_dir})
        set(test_source "${test_source_dir}/${test_name}.cpp")
        
        file(WRITE ${test_source} "#include \"${header_name}\"\n")
        
        # Add as a library target (compile-only, no link)
        add_library(${test_name} OBJECT EXCLUDE_FROM_ALL ${test_source})
        target_include_directories(${test_name} PRIVATE ${ARG_HEADERS_DIR})
        set_target_properties(${test_name} PROPERTIES
            CXX_STANDARD ${ARG_CXX_STANDARD}
            CXX_STANDARD_REQUIRED ON
        )
        
        # Add a test that builds this target
        add_test(
            NAME ${test_name}
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${test_name}
        )
    endforeach()
    
    # Add a convenience target to build all header tests
    add_custom_target(${ARG_TARGET}
        COMMENT "Checking all headers are self-contained"
    )
    
endfunction()
