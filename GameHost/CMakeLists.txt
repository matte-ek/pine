cmake_minimum_required(VERSION 3.24)
project(GameHost)

set(CMAKE_CXX_STANDARD 17)

include_directories(src include ${CMAKE_SOURCE_DIR}/Engine/src ${CMAKE_SOURCE_DIR}/Engine/include /usr/include/freetype2 /usr/include/mono-2.0)
link_directories(lib)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_definitions(-D GLM_ENABLE_EXPERIMENTAL)

add_executable(GameHost ${SOURCES})

target_link_libraries(GameHost Engine mono-2.0)
