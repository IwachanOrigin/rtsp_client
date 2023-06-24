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

# Filter
source_group("Ui Files" FILES ${UI_FILES})
source_group("Qrc Files" FILES ${QRC_FILES})

# Set the live555 bin path
set(LIVE555_PATH ${CMAKE_CURRENT_SOURCE_DIR}/assets/live555)
set(LIVE555_PATH_BIN ${LIVE555_PATH}/bin)
set(LIVE555_PATH_INCLUDE ${LIVE555_PATH}/include)
set(LIVE555_PATH_LIB ${LIVE555_PATH}/lib)

# set cmake prefix path
set(CMAKE_PREFIX_PATH ${LIVE555_PATH_LIB}/cmake/live555)

# Find the OpenCV
find_package(live555 REQUIRED)

# Set up the copy dll list
set(EXTERNAL_DLLS ${LIVE555_PATH_BIN}/Release/live555.dll)

# Set the live555 include directory
include_directories(${LIVE555_PATH_INCLUDE})

add_subdirectory(main)
