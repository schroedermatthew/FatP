#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Boost::fiber_numa" for configuration "Release"
set_property(TARGET Boost::fiber_numa APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Boost::fiber_numa PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/boost_fiber_numa-vc143-mt-x64-1_88.lib"
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "Boost::filesystem"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/boost_fiber_numa-vc143-mt-x64-1_88.dll"
  )

list(APPEND _cmake_import_check_targets Boost::fiber_numa )
list(APPEND _cmake_import_check_files_for_Boost::fiber_numa "${_IMPORT_PREFIX}/lib/boost_fiber_numa-vc143-mt-x64-1_88.lib" "${_IMPORT_PREFIX}/bin/boost_fiber_numa-vc143-mt-x64-1_88.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
