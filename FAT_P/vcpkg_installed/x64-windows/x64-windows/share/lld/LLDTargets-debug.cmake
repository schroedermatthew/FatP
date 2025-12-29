#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "lldCommon" for configuration "Debug"
set_property(TARGET lldCommon APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(lldCommon PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/lldCommon.lib"
  )

list(APPEND _cmake_import_check_targets lldCommon )
list(APPEND _cmake_import_check_files_for_lldCommon "${_IMPORT_PREFIX}/debug/lib/lldCommon.lib" )

# Import target "lld" for configuration "Debug"
set_property(TARGET lld APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(lld PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/tools/llvm/lld.exe"
  )

list(APPEND _cmake_import_check_targets lld )
list(APPEND _cmake_import_check_files_for_lld "${_IMPORT_PREFIX}/tools/llvm/lld.exe" )

# Import target "lldCOFF" for configuration "Debug"
set_property(TARGET lldCOFF APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(lldCOFF PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/lldCOFF.lib"
  )

list(APPEND _cmake_import_check_targets lldCOFF )
list(APPEND _cmake_import_check_files_for_lldCOFF "${_IMPORT_PREFIX}/debug/lib/lldCOFF.lib" )

# Import target "lldELF" for configuration "Debug"
set_property(TARGET lldELF APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(lldELF PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/lldELF.lib"
  )

list(APPEND _cmake_import_check_targets lldELF )
list(APPEND _cmake_import_check_files_for_lldELF "${_IMPORT_PREFIX}/debug/lib/lldELF.lib" )

# Import target "lldMachO" for configuration "Debug"
set_property(TARGET lldMachO APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(lldMachO PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/lldMachO.lib"
  )

list(APPEND _cmake_import_check_targets lldMachO )
list(APPEND _cmake_import_check_files_for_lldMachO "${_IMPORT_PREFIX}/debug/lib/lldMachO.lib" )

# Import target "lldMinGW" for configuration "Debug"
set_property(TARGET lldMinGW APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(lldMinGW PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/lldMinGW.lib"
  )

list(APPEND _cmake_import_check_targets lldMinGW )
list(APPEND _cmake_import_check_files_for_lldMinGW "${_IMPORT_PREFIX}/debug/lib/lldMinGW.lib" )

# Import target "lldWasm" for configuration "Debug"
set_property(TARGET lldWasm APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(lldWasm PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/lldWasm.lib"
  )

list(APPEND _cmake_import_check_targets lldWasm )
list(APPEND _cmake_import_check_files_for_lldWasm "${_IMPORT_PREFIX}/debug/lib/lldWasm.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
