#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "libevent::core" for configuration "Debug"
set_property(TARGET libevent::core APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(libevent::core PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/event_cored.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/event_cored.dll"
  )

list(APPEND _cmake_import_check_targets libevent::core )
list(APPEND _cmake_import_check_files_for_libevent::core "${_IMPORT_PREFIX}/debug/lib/event_cored.lib" "${_IMPORT_PREFIX}/debug/bin/event_cored.dll" )

# Import target "libevent::extra" for configuration "Debug"
set_property(TARGET libevent::extra APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(libevent::extra PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/event_extrad.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/event_extrad.dll"
  )

list(APPEND _cmake_import_check_targets libevent::extra )
list(APPEND _cmake_import_check_files_for_libevent::extra "${_IMPORT_PREFIX}/debug/lib/event_extrad.lib" "${_IMPORT_PREFIX}/debug/bin/event_extrad.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
