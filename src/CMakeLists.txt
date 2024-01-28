
cmake_minimum_required(VERSION 3.10)

# set the project name
project(rtspClient CXX)

# output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# output compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
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

find_package(Git REQUIRED)

# Define the version numbers
set(SOFTWARE_VERSION_MAJOR 1)
set(SOFTWARE_VERSION_MINOR 0)
set(SOFTWARE_VERSION_PATCH 0)

# Get git hash
execute_process(
  COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_SHA1
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Configure version.h.in to create version.h
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h
)

if (WIN32)
  if(DEFINED FFMPEG_PATH)
    list(APPEND CMAKE_PREFIX_PATH ${FFMPEG_PATH})
  else()
    message(FATAL_ERROR "!!!!!!!! FFMPEG_PATH IS NOT SET !!!!!!!!")
  endif(DEFINED FFMPEG_PATH)

  if(DEFINED SDL2_PATH)
    list(APPEND CMAKE_PREFIX_PATH ${SDL2_PATH})
  else()
    message(FATAL_ERROR "!!!!!!!! SDL2_PATH IS NOT SET !!!!!!!!")
  endif(DEFINED SDL2_PATH)

  if(DEFINED LIVE555_PATH)
    list(APPEND CMAKE_PREFIX_PATH ${LIVE555_PATH})
  else()
    message(FATAL_ERROR "!!!!!!!! LIVE555_PATH IS NOT SET !!!!!!!!")
  endif(DEFINED LIVE555_PATH)

  if(DEFINED OPENSSL_PATH)
    list(APPEND CMAKE_PREFIX_PATH ${OPENSSL_PATH})
  else()
    message(FATAL_ERROR "!!!!!!!! OPENSSL_PATH IS NOT SET !!!!!!!!")
  endif(DEFINED OPENSSL_PATH)

  set(FFMPEG_PATH_BIN ${FFMPEG_PATH}/bin)
  set(FFMPEG_PATH_INC ${FFMPEG_PATH}/include)
  set(FFMPEG_PATH_LIB ${FFMPEG_PATH}/lib)

  # Set live555 various
  set(LIVE555_PATH_BIN ${LIVE555_PATH}/bin)
  set(LIVE555_PATH_INC ${LIVE555_PATH}/include)
  set(LIVE555_PATH_LIB ${LIVE555_PATH}/lib)
  set(LIVE555_INC_BASICUSAGEENV ${LIVE555_PATH_INC}/basicusageenvironment)
  set(LIVE555_INC_EPOLLTASKSCHE ${LIVE555_PATH_INC}/epolltaskscheduler)
  set(LIVE555_INC_GROUPSOCK ${LIVE555_PATH_INC}/groupsock)
  set(LIVE555_INC_LIVEMEDIA ${LIVE555_PATH_INC}/livemedia)
  set(LIVE555_INC_USAGEENV ${LIVE555_PATH_INC}/usageenvironment)

  # Find the OpenSSL, Live555, SDL2
  find_package(OpenSSL REQUIRED)
  find_package(live555 REQUIRED)
  find_package(SDL2 REQUIRED)

  # Set up the copy dll list
  set(EXTERNAL_DLLS
    ${LIVE555_PATH_BIN}/$<IF:$<CONFIG:Debug>,Debug,Release>/live555.dll
    ${OPENSSL_PATH}/libcrypto-3-x64.dll
    ${OPENSSL_PATH}/libssl-3-x64.dll
    ${FFMPEG_PATH_BIN}/avcodec-60.dll
    ${FFMPEG_PATH_BIN}/avdevice-60.dll
    ${FFMPEG_PATH_BIN}/avfilter-9.dll
    ${FFMPEG_PATH_BIN}/avformat-60.dll
    ${FFMPEG_PATH_BIN}/avutil-58.dll
    ${FFMPEG_PATH_BIN}/swresample-4.dll
    ${FFMPEG_PATH_BIN}/swscale-7.dll
    ${SDL2_BINDIR}/SDL2.dll
  )

  # Set the ffmpeg, sdl2, version.h include directory
  include_directories(
    ${LIVE555_INC_BASICUSAGEENV}
    ${LIVE555_INC_EPOLLTASKSCHE}
    ${LIVE555_INC_GROUPSOCK}
    ${LIVE555_INC_LIVEMEDIA}
    ${LIVE555_INC_USAGEENV}
    ${FFMPEG_PATH_INC}
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR} # For version.h
  )
else()
  # Linux
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(
    FFMPEG
    REQUIRED
    IMPORTED_TARGET
    libavcodec
    libavdevice
    libavfilter
    libavformat
    libavutil
    libswresample
    libswscale
  )
  find_package(SDL2 REQUIRED)
  # Set the ffmpeg, sdl2, version.h include directory
  include_directories(
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR} # For version.h
  )
endif()

add_subdirectory(main)


