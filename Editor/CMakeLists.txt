cmake_minimum_required(VERSION 3.24)
project(Editor)

set(CMAKE_CXX_STANDARD 17)

include_directories(src include imgui ${CMAKE_SOURCE_DIR}/Engine/src ${CMAKE_SOURCE_DIR}/Engine/include /usr/include/freetype2)

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE IMGUI_SOURCES "imgui/*.cpp")

add_executable(Editor ${SOURCES} ${IMGUI_SOURCES})

target_link_libraries(Editor Engine freetype)
