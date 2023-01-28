cmake_minimum_required(VERSION 3.24)
project(Editor)

set(CMAKE_CXX_STANDARD 17)

find_package(Freetype REQUIRED)

include_directories(src ${CMAKE_SOURCE_DIR}/Engine/src ImGui)

file(GLOB_RECURSE IMGUI_SOURCES "ImGui/*.cpp")

add_executable(Editor ${IMGUI_SOURCES} src/Application.cpp src/Gui/Gui.hpp src/Gui/Gui.cpp src/Rendering/RenderHandler.hpp src/Rendering/RenderHandler.cpp src/Gui/Panels/GameViewport/GameViewportPanel.hpp src/Gui/Panels/GameViewport/GameViewportPanel.cpp src/Gui/Panels/LevelViewport/LevelViewportPanel.cpp src/Gui/Panels/EntityList/EntityListPanel.hpp src/Gui/Panels/EntityList/EntityListPanel.cpp src/Gui/Panels/AssetBrowser/AssetBrowserPanel.cpp src/Gui/Panels/Properties/PropertiesPanel.hpp src/Gui/Panels/Properties/PropertiesPanel.cpp)

target_link_libraries(Editor Engine Freetype::Freetype)