cmake_minimum_required(VERSION 3.24)

# ImGui
file(GLOB_RECURSE IMGUI_SOURCES "imgui/*.cpp")

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC imgui /usr/include/freetype2)

# Material Icons
add_library(material-icons INTERFACE)

target_include_directories(material-icons INTERFACE material-icons)

# Perlin Noise
add_library(perlin-noise INTERFACE)

target_include_directories(perlin-noise INTERFACE perlin-noise)

# PhysX
add_library(physx INTERFACE)

target_include_directories(physx INTERFACE physx/include physx/include/physx)
target_link_libraries(physx INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysXExtensions_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysX_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysXPvdSDK_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysXVehicle2_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysXCharacterKinematic_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysXCooking_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysXCommon_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysXFoundation_static_64.a"
)

# NVTT
add_library(nvtt INTERFACE)

target_include_directories(nvtt INTERFACE nvtt/include)
target_link_libraries(nvtt INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/nvtt/lib/libnvtt.so.30205")