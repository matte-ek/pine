cmake_minimum_required(VERSION 3.24)
project(Editor)

set(CMAKE_CXX_STANDARD 23)

include_directories(src ${CMAKE_SOURCE_DIR}/Engine/src /usr/include/mono-2.0)
link_directories(lib)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_definitions(-D GLM_ENABLE_EXPERIMENTAL)

add_executable(Editor ${SOURCES})

target_link_libraries(Editor Engine mono-2.0 freetype imgui material-icons)
