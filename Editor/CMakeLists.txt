cmake_minimum_required(VERSION 3.24)
project(Editor)

set(CMAKE_CXX_STANDARD 17)

include_directories(src include imgui ${CMAKE_SOURCE_DIR}/Engine/src ${CMAKE_SOURCE_DIR}/Engine/include /usr/include/freetype2 /usr/include/mono-2.0)
link_directories(lib)

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE IMGUI_SOURCES "imgui/*.cpp")

add_definitions(-D GLM_ENABLE_EXPERIMENTAL)

add_executable(Editor ${SOURCES} ${IMGUI_SOURCES}
        src/Gui/Panels/Properties/EntityPropertiesRenderer/ComponentPropertiesRenderer/ComponentPropertiesRenderer.cpp
        src/Gui/Panels/Properties/EntityPropertiesRenderer/ComponentPropertiesRenderer/ComponentPropertiesRenderer.hpp
        src/Utilities/Assets/AssetUtilities.cpp
        src/Utilities/Assets/AssetUtilities.hpp
        src/Projects/Projects.cpp
        src/Projects/Projects.hpp
        src/Gui/Panels/AssetBrowser/AssetHierarchy/AssetHierarchy.cpp
        src/Gui/Panels/AssetBrowser/AssetHierarchy/AssetHierarchy.hpp
        src/Gui/Dialogs/Asset/AssetDialogs.cpp
        src/Gui/Dialogs/Asset/AssetDialogs.hpp)

target_link_libraries(Editor Engine mono-2.0)
