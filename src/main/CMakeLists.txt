
set(main_src
  audiorenderer.cpp
  audiorenderer.h
  audioresamplingstate.cpp
  audioresamplingstate.h
  audiosink.cpp
  audiosink.h
  basertspclient.cpp
  basertspclient.h
  framecontainer.cpp
  framecontainer.h
  framequeue.cpp
  framequeue.h
  main.cpp
  rtspcontroller.cpp
  rtspcontroller.h
  streamclientstate.cpp
  streamclientstate.h
  videorenderer.cpp
  videorenderer.h
  videosink.cpp
  videosink.h
)

add_executable(
  ${PROJECT_NAME}
  ${main_src}
)

# Link
if(WIN32)
  target_link_libraries(
    ${PROJECT_NAME}
    ${FFMPEG_PATH_LIB}/avcodec.lib
    ${FFMPEG_PATH_LIB}/avdevice.lib
    ${FFMPEG_PATH_LIB}/avfilter.lib
    ${FFMPEG_PATH_LIB}/avformat.lib
    ${FFMPEG_PATH_LIB}/avutil.lib
    ${FFMPEG_PATH_LIB}/swresample.lib
    ${FFMPEG_PATH_LIB}/swscale.lib
    live555::live555
    SDL2::SDL2
  )
else()
  # Linux
  target_link_libraries(
    ${PROJECT_NAME}
    PkgConfig::FFMPEG
    SDL2::SDL2
    live555::live555
  )
endif()

if(WIN32)
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
endif()

