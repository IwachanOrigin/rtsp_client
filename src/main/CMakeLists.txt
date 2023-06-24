
set(main_src
  main.cpp
)

add_executable(
  ${PROJECT_NAME}
  ${main_src}
)

# Link
target_link_libraries(
  ${PROJECT_NAME}
  ${LIVE555_PATH_LIB}/Release/live555.lib
)

# Copy dlls
foreach(DLL IN LISTS EXTERNAL_DLLS)
  # Get the file name of the DLL
  get_filename_component(DLL_FILENAME "${DLL}" NAME)

  # Command to copy the DLL file
  add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${DLL}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${DLL_FILENAME}"
    COMMENT "Copying ${DLL_FILENAME} to output directory."
  )
endforeach()

