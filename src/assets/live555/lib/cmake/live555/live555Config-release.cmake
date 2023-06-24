#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "live555::live555" for configuration "Release"
set_property(TARGET live555::live555 APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(live555::live555 PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/Release/live555.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/Release/live555.dll"
  )

list(APPEND _cmake_import_check_targets live555::live555 )
list(APPEND _cmake_import_check_files_for_live555::live555 "${_IMPORT_PREFIX}/lib/Release/live555.lib" "${_IMPORT_PREFIX}/bin/Release/live555.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
