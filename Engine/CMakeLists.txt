cmake_minimum_required(VERSION 3.24)
project(Engine)

set(CMAKE_CXX_STANDARD 17)

include_directories(src include /usr/include/mono-2.0)
link_directories(lib)

file(GLOB_RECURSE SOURCES "src/*.cpp")

find_package(fmt CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(GLEW CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(OpenAL CONFIG REQUIRED)
find_package(FLAC CONFIG REQUIRED)
find_package(box2d CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-omniverse-physx-sdk CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)

add_definitions(-D GLM_ENABLE_EXPERIMENTAL)

add_library(Engine ${SOURCES}
        src/Pine/World/Components/AudioSource/AudioSource.hpp
        src/Pine/World/Components/AudioSource/AudioSource.cpp
        src/Pine/World/Components/AudioListener/AudioListener.hpp
        src/Pine/World/Components/AudioListener/AudioListener.cpp
        src/Pine/Rendering/Features/Shadows/Shadows.hpp
        src/Pine/Rendering/Features/Shadows/Shadows.cpp)

target_link_libraries(Engine glfw GLEW::GLEW assimp::assimp imgui::imgui imguizmo::imguizmo fmt::fmt OpenAL::OpenAL FLAC::FLAC++ box2d::box2d nlohmann_json::nlohmann_json unofficial::omniverse-physx-sdk::sdk glm::glm mono-2.0-sgen)