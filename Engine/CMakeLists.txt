cmake_minimum_required(VERSION 3.24)
project(Engine)

set(CMAKE_CXX_STANDARD 17)

include_directories(src)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

add_library(Engine ${SOURCES})

target_link_libraries(Engine
        PUBLIC
        GLEW
        GL
        assimp
        fmt
        nethost
        openal
        glfw
        jpeg
        png
        z
        physx
        nvtt
        perlin-noise
)
