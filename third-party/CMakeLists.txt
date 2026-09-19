cmake_minimum_required(VERSION 3.24)

# ImGui
file(GLOB_RECURSE IMGUI_SOURCES "imgui/*.cpp")

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC imgui /usr/include/freetype2)

# cpp-httplib
add_library(cpp-httplib INTERFACE)

target_include_directories(cpp-httplib INTERFACE cpp-httplib)

# Material Icons
add_library(material-icons INTERFACE)

target_include_directories(material-icons INTERFACE material-icons)

# Perlin Noise
add_library(perlin-noise INTERFACE)

target_include_directories(perlin-noise INTERFACE perlin-noise)

# PhysX
add_library(physx INTERFACE)

target_include_directories(physx INTERFACE physx/include physx/include/physx)
# Wrap the PhysX static archives in a linker group. These libraries reference each other
# (e.g. CharacterKinematic depends on symbols in Extensions), so a plain left-to-right scan is
# order-sensitive; --start-group/--end-group makes the linker re-scan until all symbols resolve.
target_link_libraries(physx INTERFACE
        -Wl,--start-group
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysXExtensions_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysX_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysXPvdSDK_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysXVehicle2_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysXCharacterKinematic_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysXCooking_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysXCommon_static_64.a"
        "${CMAKE_CURRENT_SOURCE_DIR}/physx/lib/libPhysXFoundation_static_64.a"
        -Wl,--end-group
)

# NVTT
add_library(nvtt INTERFACE)

target_include_directories(nvtt INTERFACE nvtt/include)
target_link_libraries(nvtt INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/nvtt/lib/libnvtt.so.30205")

# .NET hosting
#
# The engine starts CoreCLR itself to run Pine.dll - see Pine::Script::Runtime. nethost is the
# small library that locates libhostfxr at runtime, and it ships inside the SDK rather than as a
# system package, under a pack directory named after the runtime identifier.
#
# Do not spell that identifier out. It is "linux-x64" on the SDKs Microsoft publishes and
# "arch-x64" on the distro-packaged one, so the directory is globbed and the newest pack wins.
find_program(DOTNET_EXECUTABLE dotnet REQUIRED)

file(REAL_PATH "${DOTNET_EXECUTABLE}" DOTNET_EXECUTABLE_REAL)
get_filename_component(DOTNET_ROOT "${DOTNET_EXECUTABLE_REAL}" DIRECTORY)

file(GLOB NETHOST_HEADERS
        "${DOTNET_ROOT}/packs/Microsoft.NETCore.App.Host.*/*/runtimes/*/native/nethost.h")

if (NOT NETHOST_HEADERS)
    message(FATAL_ERROR
            "No Microsoft.NETCore.App.Host pack under ${DOTNET_ROOT}/packs. The .NET SDK provides "
            "the headers and library the engine hosts the scripting runtime with; installing the "
            "runtime alone is not enough to build.")
endif ()

list(SORT NETHOST_HEADERS COMPARE NATURAL)
list(GET NETHOST_HEADERS -1 NETHOST_HEADER)

get_filename_component(NETHOST_DIR "${NETHOST_HEADER}" DIRECTORY)

message(STATUS "Hosting the scripting runtime with ${NETHOST_DIR}")

add_library(nethost INTERFACE)

target_include_directories(nethost INTERFACE "${NETHOST_DIR}")
# The static library, so a built engine does not also need this pack directory on its library
# path. It dlopen()s libhostfxr, hence the dynamic loader.
target_link_libraries(nethost INTERFACE "${NETHOST_DIR}/libnethost.a" ${CMAKE_DL_LIBS})
