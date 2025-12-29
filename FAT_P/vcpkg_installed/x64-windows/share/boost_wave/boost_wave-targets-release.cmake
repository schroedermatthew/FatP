#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Boost::wave" for configuration "Release"
set_property(TARGET Boost::wave APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Boost::wave PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/boost_wave-vc143-mt-x64-1_88.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/boost_wave-vc143-mt-x64-1_88.dll"
  )

list(APPEND _cmake_import_check_targets Boost::wave )
list(APPEND _cmake_import_check_files_for_Boost::wave "${_IMPORT_PREFIX}/lib/boost_wave-vc143-mt-x64-1_88.lib" "${_IMPORT_PREFIX}/bin/boost_wave-vc143-mt-x64-1_88.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
