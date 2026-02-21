cmake_minimum_required(VERSION 3.24)
project(EngineCli)

set(CMAKE_CXX_STANDARD 17)

include_directories(src include ${CMAKE_SOURCE_DIR}/Engine/src ${CMAKE_SOURCE_DIR}/Engine/include /usr/include/freetype2 /usr/include/mono-2.0)
link_directories(lib)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_definitions(-D GLM_ENABLE_EXPERIMENTAL)

add_executable(EngineCli ${SOURCES})

target_link_libraries(EngineCli Engine mono-2.0)
