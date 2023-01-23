cmake_minimum_required(VERSION 3.24)
project(Sandbox)

set(CMAKE_CXX_STANDARD 17)

include_directories(src ${CMAKE_SOURCE_DIR}/Engine/src)

add_executable(Sandbox src/Application.cpp)

target_link_libraries(Sandbox Engine)