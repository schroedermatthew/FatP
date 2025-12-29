#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Boost::stacktrace_windbg" for configuration "Debug"
set_property(TARGET Boost::stacktrace_windbg APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(Boost::stacktrace_windbg PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/boost_stacktrace_windbg-vc143-mt-gd-x64-1_88.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/boost_stacktrace_windbg-vc143-mt-gd-x64-1_88.dll"
  )

list(APPEND _cmake_import_check_targets Boost::stacktrace_windbg )
list(APPEND _cmake_import_check_files_for_Boost::stacktrace_windbg "${_IMPORT_PREFIX}/debug/lib/boost_stacktrace_windbg-vc143-mt-gd-x64-1_88.lib" "${_IMPORT_PREFIX}/debug/bin/boost_stacktrace_windbg-vc143-mt-gd-x64-1_88.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
