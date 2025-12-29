#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "gflags_shared" for configuration "Release"
set_property(TARGET gflags_shared APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(gflags_shared PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/gflags.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/gflags.dll"
  )

list(APPEND _cmake_import_check_targets gflags_shared )
list(APPEND _cmake_import_check_files_for_gflags_shared "${_IMPORT_PREFIX}/lib/gflags.lib" "${_IMPORT_PREFIX}/bin/gflags.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
