
cmake_minimum_required(VERSION 3.10)

# set the project name
project(rtspClient CXX)

# output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# output compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -DUNICODE -D_UNICODE)

# Visual StudioのfilteringをON
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Keep the auto-generated files together in the Visual Studio source tree.
# Because by default it it added to Source Files in the Visual Studio source tree.
# This is very hard to see.
set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER AutoGenFiles)
set_property(GLOBAL PROPERTY AUTOGEN_SOURCE_GROUP AutoGenFiles)

# Set the ffmpeg bin path
set(FFMPEG_PATH ${CMAKE_CURRENT_SOURCE_DIR}/_assets/ffmpeg-n6.0-latest-win64-lgpl-shared-6.0)
set(FFMPEG_PATH_BIN ${FFMPEG_PATH}/bin)
set(FFMPEG_PATH_INC ${FFMPEG_PATH}/include)
set(FFMPEG_PATH_LIB ${FFMPEG_PATH}/lib)

# Set up the copy dll list
set(EXTERNAL_DLLS
  ${FFMPEG_PATH_BIN}/avcodec-60.dll
  ${FFMPEG_PATH_BIN}/avdevice-60.dll
  ${FFMPEG_PATH_BIN}/avfilter-9.dll
  ${FFMPEG_PATH_BIN}/avformat-60.dll
  ${FFMPEG_PATH_BIN}/avutil-58.dll
  ${FFMPEG_PATH_BIN}/swresample-4.dll
  ${FFMPEG_PATH_BIN}/swscale-7.dll
)

# Set the opencv include directory
include_directories(${FFMPEG_PATH_INC})

add_subdirectory(main)
