
set(hlsl_srcs
  common.fxh
  videoVS.hlsl
  videoPS.hlsl
)

# options for shader debug mode:  /Zi /Zss /Od /FdColorConvPS.pdb
set_source_files_properties(videoVS.hlsl
  PROPERTIES VS_SHADER_FLAGS "/Vng_videoVS /Fh\"${CMAKE_CURRENT_SOURCE_DIR}/videoVS.h\""
  VS_SHADER_TYPE Vertex
  VS_SHADER_MODEL 4.0
  VS_SHADER_ENTRYPOINT VS )
set_source_files_properties(videoPS.hlsl
  PROPERTIES VS_SHADER_FLAGS "/Vng_videoPS /Fh\"${CMAKE_CURRENT_SOURCE_DIR}/videoPS.h\""
  VS_SHADER_TYPE Pixel
  VS_SHADER_MODEL 4.0
  VS_SHADER_ENTRYPOINT PS )

add_custom_target(hlsl SOURCES ${hlsl_srcs})

set(main_src
  ffmpegdecoder.h
  ffmpegdecoder.cpp
  dx11manager.cpp
  dx11manager.h
  main.cpp
  mesh.cpp
  mesh.h
  pipeline.cpp
  pipeline.h
  SimpleMath.cpp
  SimpleMath.h
  SimpleMath.inl
  stringhelper.h
  stdafx.cpp
  stdafx.h
  timer.h
  win32messagehandler.cpp
  win32messagehandler.h
)

add_executable(${PROJECT_NAME} ${main_src})
add_dependencies(${PROJECT_NAME} hlsl)

# Copy dlls
foreach(DLL IN LISTS EXTERNAL_DLLS)
  # Get the file name of the DLL
  get_filename_component(DLL_FILENAME "${DLL}" NAME)

  # Command to copy the DLL file
  add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${DLL}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${DLL_FILENAME}"
    COMMENT "Copying external DLL to output directory"
  )
endforeach()

