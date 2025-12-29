#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Boost::log_setup" for configuration "Debug"
set_property(TARGET Boost::log_setup APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(Boost::log_setup PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/boost_log_setup-vc143-mt-gd-x64-1_88.lib"
  IMPORTED_LINK_DEPENDENT_LIBRARIES_DEBUG "Boost::date_time;Boost::filesystem"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/boost_log_setup-vc143-mt-gd-x64-1_88.dll"
  )

list(APPEND _cmake_import_check_targets Boost::log_setup )
list(APPEND _cmake_import_check_files_for_Boost::log_setup "${_IMPORT_PREFIX}/debug/lib/boost_log_setup-vc143-mt-gd-x64-1_88.lib" "${_IMPORT_PREFIX}/debug/bin/boost_log_setup-vc143-mt-gd-x64-1_88.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
