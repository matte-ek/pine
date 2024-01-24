cmake_minimum_required(VERSION 3.24)
project(Editor)

set(CMAKE_CXX_STANDARD 17)

include_directories(src include imgui ${CMAKE_SOURCE_DIR}/Engine/src ${CMAKE_SOURCE_DIR}/Engine/include /usr/include/freetype2)

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE IMGUI_SOURCES "imgui/*.cpp")

add_definitions(-D GLM_ENABLE_EXPERIMENTAL)

add_executable(Editor ${SOURCES} ${IMGUI_SOURCES}
        src/Gui/Panels/Engine/EngineAssetsPanel.hpp
        src/Gui/Panels/Engine/EngineAssetsPanel.cpp)

target_link_libraries(Editor Engine freetype)
