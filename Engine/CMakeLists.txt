cmake_minimum_required(VERSION 3.24)
project(Engine)

set(CMAKE_CXX_STANDARD 17)

include_directories(src include)

file(GLOB_RECURSE SOURCES "src/*.cpp")

find_package(ReactPhysics3D REQUIRED)

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
)

target_link_libraries(Engine glfw GLEW GL assimp fmt ReactPhysics3D::ReactPhysics3D)