cmake_minimum_required(VERSION 3.24)
project(Pine)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(third-party)

add_subdirectory(Engine)
add_subdirectory(GameHost)
add_subdirectory(Editor)
