# CMake configuration file for folly
#
# This provides the Folly::folly target, which you can depend on by adding it
# to your target_link_libraries().
#
# It also defines the following variables, although using these directly is not
# necessary if you use the Folly::folly target instead.
#  FOLLY_INCLUDE_DIR
#  FOLLY_LIBRARIES


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was folly-config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

include(CMakeFindDependencyMacro)

set_and_check(FOLLY_INCLUDE_DIRS "${PACKAGE_PREFIX_DIR}/include")
set_and_check(FOLLY_INCLUDE_DIR "${PACKAGE_PREFIX_DIR}/include")
set_and_check(FOLLY_CMAKE_DIR "${PACKAGE_PREFIX_DIR}/share/folly")

# find_dependency() ends up changing PACKAGE_PREFIX_DIR, so save
# folly's prefix directory in the FOLLY_PREFIX_DIR variable
set(FOLLY_PREFIX_DIR "${PACKAGE_PREFIX_DIR}")

# Include the folly-targets.cmake file, which is generated from our CMake rules
if (NOT TARGET Folly::folly)
  include("${FOLLY_CMAKE_DIR}/folly-targets.cmake")
endif()

# Set FOLLY_LIBRARIES from our Folly::folly target
set(FOLLY_LIBRARIES Folly::folly)

# Find folly's dependencies
find_dependency(fmt CONFIG)
find_dependency(double-conversion CONFIG)
find_dependency(FastFloat CONFIG)
set(z_vcpkg_folly_gflags_backup ${GFLAGS_USE_TARGET_NAMESPACE})
set(GFLAGS_USE_TARGET_NAMESPACE ON)
find_dependency(gflags CONFIG)
set(GFLAGS_USE_TARGET_NAMESPACE ${z_vcpkg_folly_gflags_backup})
find_dependency(glog CONFIG)
find_dependency(Libevent CONFIG)
find_dependency(ZLIB)
if("OFF")
  find_dependency(lz4 CONFIG)
endif()
if("OFF")
  find_dependency(zstd CONFIG)
endif()
if("OFF")
  find_dependency(Snappy CONFIG)
endif()
if("OFF")
  find_dependency(unofficial-sodium CONFIG)
endif()

set(Boost_USE_STATIC_LIBS "ON")
find_dependency(Boost
  COMPONENTS
    context
    filesystem
    program_options
    regex
    system
    thread
  REQUIRED
)

if (NOT folly_FIND_QUIETLY)
  message(STATUS "Found folly: ${FOLLY_PREFIX_DIR}")
endif()
