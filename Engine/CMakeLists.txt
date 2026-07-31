cmake_minimum_required(VERSION 3.24)
project(Engine)

set(CMAKE_CXX_STANDARD 23)

include_directories(src include include/physx /usr/include/mono-2.0)
link_directories(lib)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

add_library(Engine ${SOURCES})

target_link_libraries(Engine
        PUBLIC
        GLEW
        GL
        assimp
        fmt
        mono-2.0
        openal
        glfw
        jpeg
        png
        z
)

target_link_libraries(Engine
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/libnvtt.so.30205"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/libPhysXExtensions_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/libPhysX_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/libPhysXPvdSDK_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/libPhysXVehicle2_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/libPhysXCharacterKinematic_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/libPhysXCooking_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/libPhysXCommon_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/libPhysXFoundation_static_64.a"
)
