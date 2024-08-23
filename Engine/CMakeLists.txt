cmake_minimum_required(VERSION 3.24)
project(Engine)

set(CMAKE_CXX_STANDARD 17)

include_directories(src include /usr/include/mono-2.0)

file(GLOB_RECURSE SOURCES "src/*.cpp")

find_package(ReactPhysics3D REQUIRED)
find_package(OpenAL REQUIRED)
find_package(FLAC)
if(${FLAC_FOUND})
else(${FLAC_FOUND})
    ExternalProject_Add(
            FLAC
            URL "http://downloads.xiph.org/releases/flac/flac-1.4.3.tar.xz"
            CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/lib/flac/configure --prefix=<INSTALL_DIR>
            BUILD_COMMAND ${MAKE}
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/flac
            UPDATE_COMMAND ""
            INSTALL_COMMAND ""
            LOG_DOWNLOAD ON
            LOG_CONFIGURE ON
            LOG_BUILD ON
    )
endif(${FLAC_FOUND})

add_definitions(-D GLM_ENABLE_EXPERIMENTAL)

add_library(Engine ${SOURCES}
        src/Pine/Utilities/HotReload/HotReload.hpp
        src/Pine/Assets/Mesh/Mesh.hpp
        src/Pine/Assets/Mesh/Mesh.cpp
        src/Pine/Assets/Model/Model.hpp
        src/Pine/Assets/Model/Model.cpp
        src/Pine/World/Components/ModelRenderer/ModelRenderer.cpp
        src/Pine/World/Components/Collider/Collider.hpp
        src/Pine/World/Components/RigidBody/RigidBody.hpp
        src/Pine/World/Components/RigidBody2D/RigidBody2D.cpp
        src/Pine/Rendering/Renderer3D/Renderer3D.hpp
        src/Pine/Graphics/ShaderStorage/ShaderStorage.hpp
        src/Pine/Rendering/Renderer3D/ShaderStorages.hpp
        src/Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.cpp
        src/Pine/Rendering/Pipeline/Pipeline3D/Pipeline3D.hpp
        src/Pine/Rendering/Features/Skybox/Skybox.cpp
        src/Pine/Input/Input.hpp
        src/Pine/Input/Input.cpp
        src/Pine/Script/Runtime/ScriptingRuntime.hpp
        src/Pine/Script/Runtime/ScriptingRuntime.cpp
        src/Pine/Script/Scripts/ScriptData.hpp
        src/Pine/Script/Scripts/ScriptField.hpp
        src/Pine/Script/ScriptManager.hpp
        src/Pine/Script/ScriptManager.cpp
        src/Pine/World/Components/Script/ScriptComponent.hpp
        src/Pine/World/Components/Script/ScriptComponent.cpp
        src/Pine/Script/Factory/ScriptObjectFactory.hpp
        src/Pine/Script/Factory/ScriptObjectFactory.cpp
        src/Pine/Script/Interfaces/Interfaces.hpp
        src/Pine/Script/Interfaces/ScriptInterfaceEntity.cpp
        src/Pine/Script/Interfaces/ScriptInterfaceLog.cpp
        src/Pine/Script/Interfaces/ScriptInterfaceAsset.cpp
        src/Pine/Script/Interfaces/ScriptInterfaceInput.cpp
)


target_link_libraries(Engine glfw GLEW GL assimp fmt ReactPhysics3D::ReactPhysics3D mono-2.0 OpenAL::OpenAL FLAC::FLAC box2d)
