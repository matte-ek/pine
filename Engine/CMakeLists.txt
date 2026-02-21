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
find_package(ZLIB REQUIRED)

add_definitions(-D GLM_ENABLE_EXPERIMENTAL)

add_library(Engine ${SOURCES}
        src/Pine/Assets/Terrain/Terrain.cpp
        src/Pine/Assets/Terrain/Terrain.hpp
        src/Pine/World/Components/TerrainRenderer/TerrainRendererComponent.cpp
        src/Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp
        src/Pine/Rendering/Features/TerrainRenderer/TerrainRenderer.cpp
        src/Pine/Rendering/Features/TerrainRenderer/TerrainRenderer.hpp
        src/Pine/Physics/Physics3D/PhysicsTerrain/PhysicsTerrain.cpp
        src/Pine/Physics/Physics3D/PhysicsTerrain/PhysicsTerrain.hpp
        src/Pine/Core/Serialization/Serialization.cpp
        src/Pine/Core/Serialization/Serialization.hpp
        src/Pine/Core/Span/Span.hpp
        src/Pine/Core/Guid/Guid.cpp
        src/Pine/Core/Guid/Guid.hpp
        src/Pine/Assets/Texture2D/Importer/TextureImporter.cpp
        src/Pine/Assets/Texture2D/Importer/TextureImporter.hpp
        src/Pine/Assets/Shader/Importer/ShaderImporter.cpp
        src/Pine/Assets/Shader/Importer/ShaderImporter.hpp
        src/Pine/Assets/Model/Importer/ModelImporter.cpp
        src/Pine/Assets/Model/Importer/ModelImporter.hpp)

if(TARGET unofficial::omniverse-physx-sdk::gpu-library)
    if(UNIX)
        # Add rpath setting to find .so libraries on unix based systems
        set_target_properties(Engine PROPERTIES
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "$ORIGIN"
        )
    endif()
    add_custom_command(TARGET Engine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:unofficial::omniverse-physx-sdk::gpu-library>
            $<TARGET_FILE_DIR:Engine>)
    if(WIN32)
        add_custom_command(TARGET Engine POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:unofficial::omniverse-physx-sdk::gpu-device-library>
                $<TARGET_FILE_DIR:Engine>)
    endif()
else()
    message(WARNING "GPU acceleration library target not defined - GPU acceleration will NOT be available!")
endif()

target_link_libraries(Engine glfw GLEW::GLEW assimp::assimp imgui::imgui imguizmo::imguizmo fmt::fmt FLAC::FLAC++ box2d::box2d nlohmann_json::nlohmann_json glm::glm mono-2.0 unofficial::omniverse-physx-sdk::sdk OpenAL::OpenAL ZLIB::ZLIB nvtt30205 Rpcrt4)
