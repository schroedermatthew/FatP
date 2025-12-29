#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Boost::log_setup" for configuration "Release"
set_property(TARGET Boost::log_setup APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Boost::log_setup PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/boost_log_setup-vc143-mt-x64-1_88.lib"
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "Boost::date_time;Boost::filesystem"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/boost_log_setup-vc143-mt-x64-1_88.dll"
  )

list(APPEND _cmake_import_check_targets Boost::log_setup )
list(APPEND _cmake_import_check_files_for_Boost::log_setup "${_IMPORT_PREFIX}/lib/boost_log_setup-vc143-mt-x64-1_88.lib" "${_IMPORT_PREFIX}/bin/boost_log_setup-vc143-mt-x64-1_88.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
